#include "modules/SqlChanges.hpp"
#include "modules/SqlWorkbench.hpp"
#include "workspace/Project.hpp"
#include <sqlite3.h>
#include <algorithm>
#include <cstring>
#include <cmath>
#include <map>
#include <filesystem>
#include <memory>
namespace lucid::modules {
namespace {
constexpr std::size_t maxDb = 64U*1024U*1024U, maxSql = 1024U*1024U;
using Db = std::unique_ptr<sqlite3, decltype(&sqlite3_close)>;
using Stmt = std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>;
bool validBindingName(std::string_view name) {
    if (name.size()<2 || name.size()>129 || (name[0]!=':' && name[0]!='@' && name[0]!='$')) return false;
    const auto first=[](unsigned char c) { return (c>='a' && c<='z') || (c>='A' && c<='Z') || c=='_'; };
    const auto rest=[&](unsigned char c) { return first(c) || (c>='0' && c<='9'); };
    return first(static_cast<unsigned char>(name[1])) && std::all_of(name.begin()+2,name.end(),rest);
}
int authorize(void*, int action, const char* first, const char*, const char* schema, const char* trigger) {
    if (trigger) return SQLITE_DENY;
    switch (action) {
    case SQLITE_SELECT: case SQLITE_READ: case SQLITE_FUNCTION: case SQLITE_RECURSIVE: return SQLITE_OK;
    case SQLITE_INSERT: case SQLITE_UPDATE: case SQLITE_DELETE:
        return first && std::strncmp(first,"sqlite_",7)!=0 && schema && std::strcmp(schema,"main")==0 ? SQLITE_OK : SQLITE_DENY;
    default: return SQLITE_DENY;
    }
}
void observeRow(void* context, int operation, const char* database, const char* table, sqlite3_int64 rowid) noexcept {
    auto& result=*static_cast<SqlChangePreview*>(context);
    try {
        if (!database || std::strcmp(database,"main")!=0 || !table || std::strlen(table)>512 || result.observedRows.size()>=200) { result.observedRowsTruncated=true; return; }
        SqlRowOperation kind;
        switch (operation) {
        case SQLITE_INSERT: kind=SqlRowOperation::Insert; break;
        case SQLITE_UPDATE: kind=SqlRowOperation::Update; break;
        case SQLITE_DELETE: kind=SqlRowOperation::Delete; break;
        default: result.observedRowsTruncated=true; return;
        }
        result.observedRows.push_back({kind,table,rowid});
    } catch (...) { result.observedRowsTruncated=true; }
}
struct Budget { const std::atomic_bool* cancel; unsigned calls=0; };
int progress(void* arg) { auto& b=*static_cast<Budget*>(arg); return ++b.calls>3000 || (b.cancel && b.cancel->load()); }
}
SqlChangePreview previewSqliteChange(std::string_view database, std::string_view sql, const std::atomic_bool* cancel) {
    return previewSqliteChangeBound(database,sql,{},cancel);
}
SqlChangePreview previewSqliteChangeBound(std::string_view database, std::string_view sql, const std::vector<SqlParameter>& parameters, const std::atomic_bool* cancel) {
    SqlChangePreview out; const auto fail=[&](std::string message) { out.error=std::move(message); out.original.clear(); out.changed.clear(); out.observedRows.clear(); out.observedRowsTruncated=false; return out; };
    if (cancel && cancel->load()) return fail("SQL preview cancelled");
    if (database.size()<100 || database.size()>maxDb || database.substr(0,16)!=std::string_view("SQLite format 3\0",16)) return fail("Expected a SQLite snapshot of at most 64 MiB");
    if (static_cast<unsigned char>(database[18])!=1 || static_cast<unsigned char>(database[19])!=1) return fail("WAL databases require a standalone snapshot");
    if (sql.empty() || sql.size()>maxSql || sql.find('\0')!=std::string_view::npos || classifySql(sql).kind!=SqlStatementKind::DataChange) return fail("Expected one supported data-change statement");
    if (parameters.size()>64) return fail("Too many SQL parameters");
    std::map<std::string,const SqlParameter*> bindings; std::size_t total=0;
    for (const auto& parameter:parameters) {
        if (!validBindingName(parameter.name) || !bindings.emplace(parameter.name,&parameter).second) return fail("Invalid or duplicate SQL parameter name");
        switch(parameter.type) {
        case SqlValueType::Null: case SqlValueType::Integer: break;
        case SqlValueType::Float: if (!std::isfinite(parameter.real)) return fail("SQL floating-point parameter must be finite"); break;
        case SqlValueType::Text: case SqlValueType::Blob:
            if (parameter.bytes.size()>256U*1024U || parameter.bytes.size()>1024U*1024U-total) return fail("SQL parameter values exceed size limit");
            total+=parameter.bytes.size(); break;
        default: return fail("Unsupported SQL parameter type");
        }
    }
    sqlite3* raw=nullptr; int status=sqlite3_open_v2(":memory:",&raw,SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE|SQLITE_OPEN_MEMORY,nullptr); Db db(raw,sqlite3_close);
    if (status!=SQLITE_OK) return fail("Cannot create SQLite snapshot");
    sqlite3_limit(db.get(),SQLITE_LIMIT_LENGTH,1024*1024); sqlite3_limit(db.get(),SQLITE_LIMIT_SQL_LENGTH,static_cast<int>(maxSql)); sqlite3_limit(db.get(),SQLITE_LIMIT_COLUMN,128); sqlite3_limit(db.get(),SQLITE_LIMIT_EXPR_DEPTH,200); sqlite3_limit(db.get(),SQLITE_LIMIT_COMPOUND_SELECT,100); sqlite3_limit(db.get(),SQLITE_LIMIT_VARIABLE_NUMBER,64);
    if (sqlite3_db_config(db.get(),SQLITE_DBCONFIG_TRUSTED_SCHEMA,0,nullptr)!=SQLITE_OK || sqlite3_db_config(db.get(),SQLITE_DBCONFIG_DEFENSIVE,1,nullptr)!=SQLITE_OK) return fail("Cannot secure SQLite snapshot");
    auto* bytes=static_cast<unsigned char*>(sqlite3_malloc64(maxDb)); if (!bytes) return fail("Cannot allocate snapshot"); std::memcpy(bytes,database.data(),database.size());
    status=sqlite3_deserialize(db.get(),"main",bytes,static_cast<sqlite3_int64>(database.size()),static_cast<sqlite3_int64>(maxDb),SQLITE_DESERIALIZE_FREEONCLOSE);
    if (status!=SQLITE_OK) return fail("Cannot load SQLite snapshot");
    Budget budget{cancel}; sqlite3_progress_handler(db.get(),1000,progress,&budget);
    sqlite3_update_hook(db.get(),observeRow,&out);
    if (sqlite3_exec(db.get(),"BEGIN",nullptr,nullptr,nullptr)!=SQLITE_OK) return fail("Cannot begin snapshot transaction");
    if (sqlite3_set_authorizer(db.get(),authorize,nullptr)!=SQLITE_OK) return fail("Cannot restrict SQLite snapshot");
    sqlite3_stmt* rawStmt=nullptr; const char* tail=nullptr; const std::string statement(sql);
    status=sqlite3_prepare_v3(db.get(),statement.c_str(),static_cast<int>(statement.size()),0,&rawStmt,&tail); Stmt stmt(rawStmt,sqlite3_finalize);
    if (status!=SQLITE_OK || !stmt) return fail(std::string("SQLite: ")+sqlite3_errmsg(db.get()));
    if (sqlite3_stmt_readonly(stmt.get()) || sqlite3_column_count(stmt.get())!=0) return fail("Statement must change data without RETURNING");
    sqlite3_stmt* extraRaw=nullptr; status=sqlite3_prepare_v3(db.get(),tail,-1,0,&extraRaw,nullptr); Stmt extra(extraRaw,sqlite3_finalize);
    if (status!=SQLITE_OK || extra) return fail("Only one statement is supported");
    const int bindingCount=sqlite3_bind_parameter_count(stmt.get());
    for (int i=1;i<=bindingCount;++i) {
        const char* name=sqlite3_bind_parameter_name(stmt.get(),i);
        if (!name || !validBindingName(name)) return fail("Only named SQL parameters are supported");
        const auto found=bindings.find(name); if (found==bindings.end()) return fail("Missing SQL parameter binding");
        const auto& parameter=*found->second; int bound=SQLITE_MISUSE;
        switch(parameter.type) {
        case SqlValueType::Null: bound=sqlite3_bind_null(stmt.get(),i); break;
        case SqlValueType::Integer: bound=sqlite3_bind_int64(stmt.get(),i,parameter.integer); break;
        case SqlValueType::Float: bound=sqlite3_bind_double(stmt.get(),i,parameter.real); break;
        case SqlValueType::Text: bound=sqlite3_bind_text64(stmt.get(),i,parameter.bytes.data(),parameter.bytes.size(),SQLITE_TRANSIENT,SQLITE_UTF8); break;
        case SqlValueType::Blob: bound=sqlite3_bind_blob64(stmt.get(),i,parameter.bytes.empty()?"":parameter.bytes.data(),parameter.bytes.size(),SQLITE_TRANSIENT); break;
        }
        if(bound!=SQLITE_OK) return fail("Could not bind SQL parameter");
        bindings.erase(found);
    }
    if(!bindings.empty()) return fail("Unused SQL parameter binding");
    status=sqlite3_step(stmt.get()); if (status!=SQLITE_DONE) return fail(status==SQLITE_INTERRUPT ? "SQL preview cancelled or instruction limit reached" : std::string("SQLite: ")+sqlite3_errmsg(db.get()));
    out.affected=sqlite3_changes64(db.get()); stmt.reset(); extra.reset();
    if (cancel && cancel->load()) return fail("SQL preview cancelled");
    if (sqlite3_set_authorizer(db.get(),nullptr,nullptr)!=SQLITE_OK || sqlite3_exec(db.get(),"COMMIT",nullptr,nullptr,nullptr)!=SQLITE_OK) return fail("Cannot commit snapshot transaction");
    sqlite3_int64 size=0; auto* result=sqlite3_serialize(db.get(),"main",&size,0);
    if (!result || size<100 || size>static_cast<sqlite3_int64>(maxDb)) { sqlite3_free(result); return fail("Cannot serialize bounded snapshot"); }
    out.original=std::string(database); out.changed.assign(reinterpret_cast<const char*>(result),static_cast<std::size_t>(size)); sqlite3_free(result); out.sql=statement; return out;
}
bool exportSqliteChange(workspace::Project& project, std::string_view source, std::string_view destination, const SqlChangePreview& preview, bool confirmed, std::string& error, const std::atomic_bool* cancel) {
    error.clear(); if (!confirmed) { error="Explicit confirmation is required"; return false; }
    if (!preview.error.empty() || preview.original.empty() || preview.changed.empty() || preview.changed.size()>maxDb || source.empty() || destination.empty() || source.find('\0')!=source.npos || destination.find('\0')!=destination.npos) { error="Invalid SQL change preview"; return false; }
    std::filesystem::path dest; try { dest=std::filesystem::path(std::u8string(destination.begin(),destination.end())); } catch (...) { error="Invalid destination"; return false; }
    if (dest.is_absolute() || dest.has_root_name() || dest.extension()!=".db") { error="Choose a new project-relative .db file"; return false; }
    auto in=project.resolve(source,error); if(in.empty()) return false; auto output=project.resolve(destination,error); if(output.empty()) return false;
    if(in==output) { error="Destination must differ from the source"; return false; }
    // A stale/live sidecar can alter the meaning of either file on open.
    // These checks are conservative; they do not lock other processes out.
    for (const auto& path : {in, output}) for (const auto* suffix : {"-wal", "-shm", "-journal"}) {
        auto sidecar=path; sidecar += suffix; std::error_code ec;
        const auto st=std::filesystem::symlink_status(sidecar,ec);
        if (ec && ec!=std::errc::no_such_file_or_directory) { error="Cannot inspect SQLite sidecar"; return false; }
        if (std::filesystem::exists(st)) { error="SQLite sidecars must be absent; close the database and use a standalone snapshot"; return false; }
    }
    std::string current; if (!project.readFileBounded(source,current,error,maxDb,cancel)) return false;
    if(current!=preview.original) { error="Database changed since preview; preview again"; return false; }
    return project.writeFile(destination,preview.changed,error,nullptr,true,cancel);
}
}
