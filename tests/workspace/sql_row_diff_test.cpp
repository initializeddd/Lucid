#include "modules/SqlRowDiff.hpp"
#include <sqlite3.h>
#include <atomic>
#include <string>
using namespace lucid::modules;
int main(){
 sqlite3* db=nullptr; if(sqlite3_open(":memory:",&db)!=SQLITE_OK)return 1;
 if(sqlite3_exec(db,"CREATE TABLE t(id INTEGER PRIMARY KEY, v TEXT, b BLOB); INSERT INTO t VALUES(1,'old',x'0102'); CREATE TABLE shadow(rowid TEXT, _rowid_ TEXT, oid TEXT, v TEXT); INSERT INTO shadow VALUES('a','b','c','old'); CREATE TABLE \"odd\"\"name\"(v TEXT); INSERT INTO \"odd\"\"name\" VALUES('x');",nullptr,nullptr,nullptr)!=SQLITE_OK)return 2;
 sqlite3_int64 size=0; auto* data=sqlite3_serialize(db,"main",&size,0); if(!data)return 3; std::string original(reinterpret_cast<char*>(data),static_cast<std::size_t>(size)); sqlite3_free(data); sqlite3_close(db);
 auto p=previewSqliteChange(original,"UPDATE t SET v='new',b=x'0103' WHERE id=1"); if(!p.error.empty() || p.observedRows.size()!=1)return 4;
 auto diff=inspectSqlChangeRow(p,0); if(!diff.error.empty() || !diff.beforeExists || !diff.afterExists || diff.cells.size()!=3 || diff.cells[0].changed || !diff.cells[1].changed || diff.cells[1].before.value!="old" || diff.cells[1].after.value!="new" || !diff.cells[2].changed || diff.cells[2].before.type!=SqlValueType::Blob)return 5;
 auto inserted=previewSqliteChange(original,"INSERT INTO t VALUES(2,'private',x'00')"); auto added=inspectSqlChangeRow(inserted,0); if(!added.error.empty() || added.beforeExists || !added.afterExists || added.cells[1].after.value!="private")return 6;
 auto deleted=previewSqliteChange(original,"DELETE FROM t WHERE id=1"); auto removed=inspectSqlChangeRow(deleted,0); if(!removed.error.empty() || !removed.beforeExists || removed.afterExists)return 7;
 auto shadow=previewSqliteChange(original,"UPDATE shadow SET v='new'"); if(!inspectSqlChangeRow(shadow,0).error.size())return 8;
 auto quoted=previewSqliteChange(original,"UPDATE \"odd\"\"name\" SET v='new'"); if(!inspectSqlChangeRow(quoted,0).error.empty())return 9;
 auto large=previewSqliteChange(original,"UPDATE t SET v=replace(hex(zeroblob(700)), '0', 'a') WHERE id=1"); auto limited=inspectSqlChangeRow(large,0); if(!limited.error.empty() || !limited.truncated || limited.cells[1].after.value.size()!=1024 || !limited.cells[1].after.truncated)return 10;
 auto invalid=p; invalid.observedRows[0].table="t\" WHERE 1=1; DROP TABLE t;--"; if(inspectSqlChangeRow(invalid,0).error.empty())return 11;
 std::atomic_bool cancel{true}; if(inspectSqlChangeRow(p,0,&cancel).error.empty() || inspectSqlChangeRow(p,1).error.empty())return 12;
 return 0;
}
