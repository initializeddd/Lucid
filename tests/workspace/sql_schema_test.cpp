#include "modules/SqlWorkbench.hpp"
#include <sqlite3.h>
#include <algorithm>
#include <atomic>
#include <string>
using namespace lucid::modules;
static std::string bytes(sqlite3* db) { sqlite3_int64 size=0; auto* p=sqlite3_serialize(db,"main",&size,0); if (!p) return {}; std::string result(reinterpret_cast<char*>(p),static_cast<std::size_t>(size)); sqlite3_free(p); return result; }
int main() {
    sqlite3* db=nullptr; if(sqlite3_open(":memory:",&db)!=SQLITE_OK) return 1;
    if(sqlite3_exec(db,"CREATE TABLE items(id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT); CREATE INDEX idx ON items(name); CREATE VIEW dangerous AS SELECT load_extension('never') FROM items; CREATE TRIGGER t AFTER INSERT ON items BEGIN UPDATE items SET name='x'; END;",nullptr,nullptr,nullptr)!=SQLITE_OK) return 2;
    auto snapshot=bytes(db); auto r=inspectSqliteSchema(snapshot);
    if(!r.error.empty() || r.truncated || r.objects.size()!=4) return 3;
    for(const auto* type:{"table","view","index","trigger"}) if(std::none_of(r.objects.begin(),r.objects.end(),[&](const auto& o){return o.type==type && o.definitionAvailable && !o.definition.empty();})) return 4;
    for(const auto& object:r.objects) if(object.name.starts_with("sqlite_")) return 5;
    std::atomic_bool cancel{true}; auto c=inspectSqliteSchema(snapshot,&cancel); if(c.error.empty() || !c.objects.empty()) return 6;
    snapshot[18]=2; if(inspectSqliteSchema(snapshot).error.empty()) return 7;
    const auto longName=std::string(17000,'a'); const auto longSql="CREATE TABLE \""+longName+"\"(x)";
    if(sqlite3_exec(db,longSql.c_str(),nullptr,nullptr,nullptr)!=SQLITE_OK) return 10;
    const auto longSchema=inspectSqliteSchema(bytes(db));
    if(!longSchema.error.empty() || !longSchema.truncated || std::none_of(longSchema.objects.begin(),longSchema.objects.end(),[](const auto& o){return o.truncated;})) return 11;
    for(int i=0;i<501;++i){const auto sql="CREATE TABLE more"+std::to_string(i)+"(x)";if(sqlite3_exec(db,sql.c_str(),nullptr,nullptr,nullptr)!=SQLITE_OK)return 8;}
    auto large=inspectSqliteSchema(bytes(db)); if(!large.error.empty() || !large.truncated || large.objects.size()!=500) return 9;
    sqlite3_close(db); return 0;
}
