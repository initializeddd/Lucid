#include "modules/SqlRowDiff.hpp"
#include <algorithm>
#include <cctype>
namespace lucid::modules {
namespace {
bool ordinaryTable(std::string_view sql) {
    // sqlite_schema stores CREATE TABLE for ordinary tables. Reject virtual
    // tables rather than invoking their potentially special query behavior.
    const auto token=[&](std::size_t& i, std::string_view expected) {
        while(i<sql.size() && (sql[i]==' ' || sql[i]=='\t' || sql[i]=='\n' || sql[i]=='\r')) ++i;
        for(char ch:expected) { if(i==sql.size() || std::toupper(static_cast<unsigned char>(sql[i++]))!=ch) return false; }
        return i==sql.size() || !(std::isalnum(static_cast<unsigned char>(sql[i])) || sql[i]=='_');
    };
    std::size_t i=0; return token(i,"CREATE") && token(i,"TABLE");
}
std::string quoted(std::string_view value) { std::string out="\""; for(char c:value) { if(c=='\"') out+='\"'; out+=c; } out+='\"'; return out; }
bool sameName(std::string_view a,std::string_view b) {
    if(a.size()!=b.size()) return false;
    for(std::size_t i=0;i<a.size();++i) { unsigned char c=a[i]; if(c>='A' && c<='Z') c+=32; if(c!=static_cast<unsigned char>(b[i])) return false; }
    return true;
}
}
SqlRowDiff inspectSqlChangeRow(const SqlChangePreview& preview,std::size_t observedIndex,const std::atomic_bool* cancel) {
    SqlRowDiff out; const auto fail=[&](const char* error) { out.cells.clear(); out.error=error; return out; };
    if(cancel && cancel->load()) return fail("Row comparison cancelled");
    if(!preview.error.empty() || preview.original.empty() || preview.changed.empty() || observedIndex>=preview.observedRows.size()) return fail("Change preview is unavailable");
    const auto& event=preview.observedRows[observedIndex];
    if(event.table.empty() || event.table.size()>512 || event.table.find('\0')!=event.table.npos) return fail("Invalid observed table name");
    const std::vector<SqlParameter> tableParam{{":table",SqlValueType::Text,0,0,event.table}};
    for(const auto* snapshot:{&preview.original,&preview.changed}) {
        auto schema=querySqliteSnapshotBound(*snapshot,"SELECT sql FROM sqlite_schema WHERE type='table' AND name=:table",tableParam,false,cancel);
        if(!schema.error.empty() || schema.truncated || schema.rows.size()!=1 || schema.rows[0].size()!=1 || schema.rows[0][0].type!=SqlValueType::Text || schema.rows[0][0].truncated || !ordinaryTable(schema.rows[0][0].value)) return fail("Only ordinary rowid tables can be compared");
    }
    auto metadata=inspectSqliteColumns(preview.original,event.table,cancel);
    if(!metadata.error.empty() || metadata.truncated || metadata.columns.empty() || metadata.columns.size()>64) return fail("Table columns are unavailable or exceed the comparison limit");
    const char* alias=nullptr;
    for(const char* candidate:{"rowid","_rowid_","oid"}) {
        if(std::none_of(metadata.columns.begin(),metadata.columns.end(),[&](const SqlColumn& col){return sameName(col.name,candidate);})) {alias=candidate;break;}
    }
    if(!alias) return fail("Row ID aliases are shadowed by table columns");
    for(const auto& col:metadata.columns) if(col.truncated || col.hiddenKind==1 || col.name.find('\0')!=col.name.npos) return fail("Unsupported or truncated table metadata");
    const std::string query="SELECT * FROM "+quoted(event.table)+" WHERE "+alias+"=:id LIMIT 2";
    const std::vector<SqlParameter> id{{":id",SqlValueType::Integer,event.rowid,0,{}}};
    auto before=querySqliteSnapshotBound(preview.original,query,id,false,cancel);
    auto after=querySqliteSnapshotBound(preview.changed,query,id,false,cancel);
    if(!before.error.empty() || !after.error.empty() || before.rows.size()>1 || after.rows.size()>1 || before.columns!=after.columns || before.columns.size()>64) return fail("Could not compare the observed row safely");
    out.beforeExists=!before.rows.empty(); out.afterExists=!after.rows.empty();
    if(!out.beforeExists && !out.afterExists) return fail("Row is absent from both snapshots");
    if ((out.beforeExists && before.rows[0].size()!=before.columns.size()) || (out.afterExists && after.rows[0].size()!=after.columns.size())) return fail("Invalid row comparison result");
    for(std::size_t i=0;i<before.columns.size();++i) {
        SqlRowCellDiff cell; cell.column=before.columns[i];
        if(out.beforeExists) cell.before=before.rows[0][i];
        if(out.afterExists) cell.after=after.rows[0][i];
        const bool incomplete=(out.beforeExists && cell.before.truncated) || (out.afterExists && cell.after.truncated);
        cell.changed=out.beforeExists!=out.afterExists || incomplete || cell.before.type!=cell.after.type || cell.before.value!=cell.after.value;
        for(auto* v:{&cell.before,&cell.after}) if(v->value.size()>1024) { v->value.resize(1024); v->truncated=true; }
        out.truncated |= cell.before.truncated || cell.after.truncated;
        out.cells.push_back(std::move(cell));
    }
    if(cancel && cancel->load()) return fail("Row comparison cancelled");
    return out;
}
}
