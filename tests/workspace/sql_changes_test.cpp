#include "modules/SqlChanges.hpp"
#include "modules/SqlWorkbench.hpp"
#include "workspace/Project.hpp"
#include <sqlite3.h>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace lucid::modules;
int main() {
 sqlite3* db=nullptr; if(sqlite3_open(":memory:",&db)!=SQLITE_OK) return 1;
 if(sqlite3_exec(db,"CREATE TABLE t(id INTEGER PRIMARY KEY, v TEXT UNIQUE); INSERT INTO t VALUES(1,'old'),(2,'two'); CREATE TABLE audit(v); INSERT INTO audit VALUES('initial'); CREATE TABLE without_ids(k TEXT PRIMARY KEY, v TEXT) WITHOUT ROWID; INSERT INTO without_ids VALUES('a','old'); CREATE TRIGGER tr AFTER DELETE ON t BEGIN INSERT INTO audit VALUES(old.v); END;",nullptr,nullptr,nullptr)!=SQLITE_OK) return 2;
 sqlite3_int64 n=0; auto* p=sqlite3_serialize(db,"main",&n,0); if(!p) return 3; std::string original(reinterpret_cast<char*>(p),static_cast<std::size_t>(n)); sqlite3_free(p); sqlite3_close(db);
 auto preview=previewSqliteChange(original,"UPDATE t SET v='new' WHERE id=1"); if(!preview.error.empty()) {std::cerr<<preview.error<<'\n';return 4;} if(preview.affected!=1 || preview.original!=original || preview.changed==original || preview.observedRows.size()!=1 || preview.observedRows[0].operation!=SqlRowOperation::Update || preview.observedRows[0].table!="t" || preview.observedRows[0].rowid!=1 || preview.observedRowsTruncated) return 5;
 auto inserted=previewSqliteChange(original,"INSERT INTO t VALUES(3,'three')"); if(!inserted.error.empty() || inserted.observedRows.size()!=1 || inserted.observedRows[0].operation!=SqlRowOperation::Insert || inserted.observedRows[0].rowid!=3) return 17;
 auto deleted=previewSqliteChange(original,"DELETE FROM audit WHERE v='initial'"); if(!deleted.error.empty() || deleted.affected!=1 || deleted.observedRows.size()!=1 || deleted.observedRows[0].operation!=SqlRowOperation::Delete || deleted.observedRows[0].table!="audit") return 18;
 auto withoutIds=previewSqliteChange(original,"UPDATE without_ids SET v='new'"); if(!withoutIds.error.empty() || withoutIds.affected!=1 || !withoutIds.observedRows.empty()) return 19;
 std::string bulk="INSERT INTO audit VALUES "; for(int i=0;i<205;++i) { if(i) bulk+=","; bulk+="("+std::to_string(i)+")"; } auto many=previewSqliteChange(original,bulk); if(!many.error.empty() || many.affected!=205 || many.observedRows.size()!=200 || !many.observedRowsTruncated) {std::cerr<<many.error<<'\n';return 20;}
 const std::string payload="secret'; DELETE FROM t; --";
 std::vector<SqlParameter> parameters={{":value",SqlValueType::Text,0,0,payload},{"@id",SqlValueType::Integer,1,0,{}}};
 auto bound=previewSqliteChangeBound(original,"UPDATE t SET v=:value WHERE id=@id",parameters);
 if(!bound.error.empty() || bound.affected!=1 || bound.sql.find(payload)!=std::string::npos) return 21;
 auto boundRows=querySqliteSnapshot(bound.changed,"SELECT v FROM t WHERE id=1");
 if(!boundRows.error.empty() || boundRows.rows.size()!=1 || boundRows.rows[0][0].value!=payload) return 22;
 for(auto input:{std::vector<SqlParameter>{parameters[0]},std::vector<SqlParameter>{parameters[0],parameters[1],parameters[0]},std::vector<SqlParameter>{parameters[0],parameters[1],{":unused",SqlValueType::Null,0,0,{}}}}) { auto rejected=previewSqliteChangeBound(original,"UPDATE t SET v=:value WHERE id=@id",input); if(rejected.error.empty() || rejected.error.find(payload)!=std::string::npos || !rejected.changed.empty()) return 23; }
 if(previewSqliteChangeBound(original,"UPDATE t SET v=?",{}).error.empty() || previewSqliteChangeBound(original,"UPDATE t SET v=$value",parameters).error.empty() || previewSqliteChangeBound(original,"UPDATE t SET v=:value",{{":value",SqlValueType::Text,0,0,std::string(256U*1024U+1,'x')}}).error.empty()) return 24;
 auto nullBound=previewSqliteChangeBound(original,"UPDATE audit SET v=:x",{{":x",SqlValueType::Null,0,0,{}}}); if(!nullBound.error.empty() || querySqliteSnapshot(nullBound.changed,"SELECT v FROM audit").rows[0][0].type!=SqlValueType::Null) return 25;
 auto old=querySqliteSnapshot(original,"SELECT v FROM t WHERE id=1"), changed=querySqliteSnapshot(preview.changed,"SELECT v FROM t WHERE id=1"); if(!old.error.empty()||!changed.error.empty()||old.rows[0][0].value!="old"||changed.rows[0][0].value!="new") return 6;
 for(auto s:{"DELETE FROM t", "UPDATE t SET v='two' WHERE id=1", "UPDATE t SET v=?", "UPDATE t SET v='x' RETURNING id", "UPDATE t SET v='x'; DELETE FROM t", "DROP TABLE t", "UPDATE sqlite_schema SET name='x'", "UPDATE t SET v=load_extension('bad')"}) { auto failed=previewSqliteChange(original,s); if(failed.error.empty() || !failed.observedRows.empty()) {std::cerr<<s<<'\n';return 7;} }
 std::atomic_bool cancelled{true}; if(previewSqliteChange(original,"UPDATE t SET v='x'",&cancelled).error.empty()) return 8;
 auto wal=original; wal[18]=2; wal[19]=2; if(previewSqliteChange(wal,"UPDATE t SET v='x'").error.empty()) return 9;
 auto temp=std::filesystem::temp_directory_path()/("lucid-sql-changes-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())); std::filesystem::create_directories(temp); {std::ofstream f(temp/"source.db",std::ios::binary);f<<original;}
 bool created=false; std::string error; auto project=lucid::workspace::Project::openOrCreate(temp,created,error); if(!error.empty()) return 10;
 if(exportSqliteChange(project,"source.db","copy.db",preview,false,error)) return 11;
 if(!exportSqliteChange(project,"source.db","copy.db",preview,true,error)) {std::cerr<<error<<'\n'; return 12;}
 if(exportSqliteChange(project,"source.db","copy.db",preview,true,error) || exportSqliteChange(project,"source.db","source.db",preview,true,error)) return 13;
 {std::ofstream f(temp/"source.db-journal"); f << "pending";}
 if(exportSqliteChange(project,"source.db","other.db",preview,true,error)) return 15;
 std::filesystem::remove(temp/"source.db-journal");
 {std::ofstream f(temp/"other.db-wal"); f << "pending";}
 if(exportSqliteChange(project,"source.db","other.db",preview,true,error)) return 16;
 std::filesystem::remove(temp/"other.db-wal");
 {std::ofstream f(temp/"source.db",std::ios::binary|std::ios::trunc);f<<"changed";}
 if(exportSqliteChange(project,"source.db","other.db",preview,true,error)) return 14;
 std::filesystem::remove_all(temp); return 0;
}
