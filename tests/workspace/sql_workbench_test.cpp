#include "modules/SqlWorkbench.hpp"
#include "workspace/Project.hpp"
#include <sqlite3.h>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
using namespace lucid::modules;
int main() {
    if (classifySql(" -- hello\n SELECT ';' /* ; */; -- trailing\n").kind != SqlStatementKind::Query) return 1;
    if (classifySql("SELECT 1; DROP TABLE x").kind != SqlStatementKind::Unknown) return 2;
    if (classifySql("UPDATE t SET x=1").kind != SqlStatementKind::DataChange) return 3;
    if (classifySql("CREATE TABLE t(x)").kind != SqlStatementKind::SchemaChange) return 4;
    if (classifySql("BEGIN TRANSACTION").kind != SqlStatementKind::Transaction) return 5;
    if (classifySql("WITH x AS (SELECT 1) DELETE FROM t").kind != SqlStatementKind::CommonTableExpression ||
        classifySql("WITH x AS (SELECT 1) SELECT * FROM x").kind != SqlStatementKind::CommonTableExpression) return 6;
    if (classifySql("PRAGMA journal_mode=WAL").kind != SqlStatementKind::Unknown) return 7;
    if (classifySql("select 'unterminated").kind != SqlStatementKind::Unknown) return 8;
    if (classifySql("select 1 /* nested /* x */ ; */").kind != SqlStatementKind::Unknown) return 9;
    if (classifySql("select $$; DROP x$$").kind != SqlStatementKind::Unknown) return 10;
    if (classifySql("UPDATE t SET v=$value WHERE id=$id").kind != SqlStatementKind::DataChange ||
        classifySql("UPDATE t SET v=$tag$; DELETE FROM t; $tag$").kind != SqlStatementKind::Unknown ||
        classifySql("UPDATE t SET v=$9").kind != SqlStatementKind::Unknown) return 54;
    if (classifySql(" -- just comment").kind != SqlStatementKind::Empty) return 11;
    if (classifySql(std::string(1024U*1024U+1,'x')).kind != SqlStatementKind::Unknown) return 12;
    if (classifySql("SELECT1").kind != SqlStatementKind::Unknown) return 13;
    sqlite3* db = nullptr;
    if (sqlite3_open(":memory:", &db) != SQLITE_OK) return 14;
    if (sqlite3_exec(db, "CREATE TABLE items(id INTEGER PRIMARY KEY, name TEXT, data BLOB); INSERT INTO items VALUES(1, 'one', x'0001'), (2, 'two', NULL); CREATE TABLE \"weird'quoted\"(id INTEGER PRIMARY KEY, value TEXT NOT NULL DEFAULT 'x', generated INTEGER GENERATED ALWAYS AS (id*2) VIRTUAL);", nullptr, nullptr, nullptr) != SQLITE_OK) return 15;
    sqlite3_int64 size = 0; unsigned char* bytes = sqlite3_serialize(db, "main", &size, 0);
    if (!bytes) return 16;
    const std::string snapshot(reinterpret_cast<char*>(bytes), static_cast<std::size_t>(size));
    sqlite3_free(bytes); sqlite3_close(db);
    auto columns = inspectSqliteColumns(snapshot, "weird'quoted");
    if (!columns.error.empty() || columns.truncated || columns.columns.size() != 3 || columns.columns[0].name != "id" || columns.columns[0].primaryKeyOrder != 1 ||
        !columns.columns[1].notNull || !columns.columns[1].hasDefault || columns.columns[1].defaultValue != "'x'" || columns.columns[2].hiddenKind != 2) return 80;
    if (inspectSqliteColumns(snapshot, "items'); DELETE FROM items; --").error.empty() || inspectSqliteColumns(snapshot, "missing").error.empty()) return 81;
    std::atomic_bool cancelledColumns{true};
    if (inspectSqliteColumns(snapshot, "items", &cancelledColumns).error.empty() || inspectSqliteColumns(snapshot, std::string("x\0y", 3)).error.empty()) return 82;
    if (querySqliteSnapshot(snapshot, "SELECT * FROM pragma_table_xinfo('items')").error.empty()) return 83;
    auto rows = querySqliteSnapshot(snapshot, "SELECT id, name, data FROM items ORDER BY id; -- end");
    if (!rows.error.empty() || rows.columns.size() != 3 || rows.rows.size() != 2 || rows.rows[0][0].value != "1" || rows.rows[0][1].value != "one" || rows.rows[0][2].type != SqlValueType::Blob || rows.rows[0][2].value != std::string("\0\1", 2) || rows.rows[1][2].type != SqlValueType::Null) return 17;
    auto plan = querySqliteSnapshot(snapshot, "SELECT * FROM items WHERE id=1", true);
    if (!plan.error.empty() || plan.rows.empty()) return 18;
    if (querySqliteSnapshot(snapshot, "DELETE FROM items").error.empty()) return 19;
    if (querySqliteSnapshot(snapshot, "SELECT 1; SELECT 2").error.empty()) return 20;
    if (querySqliteSnapshot(snapshot, "ATTACH DATABASE ':memory:' AS x").error.empty()) return 21;
    if (querySqliteSnapshot(snapshot, "PRAGMA user_version").error.empty()) return 22;
    if (querySqliteSnapshot(snapshot, "SELECT load_extension('bad')").error.empty()) return 23;
    auto limited = querySqliteSnapshot(snapshot, "WITH RECURSIVE t(x) AS (VALUES(1) UNION ALL SELECT x+1 FROM t WHERE x<1000) SELECT x FROM t");
    if (!limited.error.empty() || !limited.truncated || limited.rows.size() != 500) return 24;
    std::atomic_bool cancelled{true};
    if (querySqliteSnapshot(snapshot, "SELECT 1", false, &cancelled).error.empty()) return 25;
    auto wal = snapshot; wal[18] = 2; wal[19] = 2;
    if (querySqliteSnapshot(wal, "SELECT 1").error.empty()) return 26;
    if (querySqliteSnapshot("not a database", "SELECT 1").error.empty()) return 27;
    auto large = querySqliteSnapshot(snapshot, "SELECT randomblob(32768)");
    if (!large.error.empty() || !large.truncated || !large.rows[0][0].truncated || large.rows[0][0].value.size() != 16384) return 28;
    auto runaway = querySqliteSnapshot(snapshot, "WITH RECURSIVE t(x) AS (VALUES(1) UNION ALL SELECT x+1 FROM t) SELECT sum(x) FROM t");
    if (runaway.error != "Query cancelled or instruction limit reached") return 29;
    const SqlParameter id{":id", SqlValueType::Integer, 2, 0, {}};
    const SqlParameter value{"@text", SqlValueType::Text, 0, 0, std::string("a\0b", 3)};
    const SqlParameter blob{"$blob", SqlValueType::Blob, 0, 0, std::string("\0\1", 2)};
    const SqlParameter emptyBlob{":empty", SqlValueType::Blob, 0, 0, {}};
    const SqlParameter real{":real", SqlValueType::Float, 0, 1.25, {}};
    const SqlParameter nullValue{":null", SqlValueType::Null, 0, 0, {}};
    auto bound = querySqliteSnapshotBound(snapshot, "SELECT name, :id, @text, $blob, :empty, :real, :null FROM items WHERE id = :id", {id, value, blob, emptyBlob, real, nullValue});
    if (!bound.error.empty() || bound.rows.size() != 1 || bound.rows[0][0].value != "two" || bound.rows[0][1].value != "2" ||
        bound.rows[0][2].value != value.bytes || bound.rows[0][3].value != blob.bytes || bound.rows[0][4].type != SqlValueType::Blob ||
        !bound.rows[0][4].value.empty() || bound.rows[0][5].type != SqlValueType::Float || bound.rows[0][6].type != SqlValueType::Null) return 30;
    auto boundPlan = querySqliteSnapshotBound(snapshot, "SELECT * FROM items WHERE id=:id", {id}, true);
    if (!boundPlan.error.empty() || boundPlan.rows.empty()) return 31;
    if (querySqliteSnapshot(snapshot, "SELECT :id").error.empty() || querySqliteSnapshotBound(snapshot, "SELECT :id", {}).error.empty()) return 32;
    if (querySqliteSnapshotBound(snapshot, "SELECT 1", {id}).error.empty() || querySqliteSnapshotBound(snapshot, "SELECT :id", {id, id}).error.empty()) return 33;
    if (querySqliteSnapshotBound(snapshot, "SELECT ?", {}).error.empty() || querySqliteSnapshotBound(snapshot, "SELECT ?1", {id}).error.empty()) return 34;
    std::vector<SqlParameter> numbered{{"?1",SqlValueType::Integer,1,0,{}},{"?2",SqlValueType::Text,0,0,"two"}};
    auto numberedRows=querySqliteSnapshotBound(snapshot,"SELECT ?2, ?1, ?1, '?3 :ignored' /* ? */",numbered);
    if(!numberedRows.error.empty() || numberedRows.rows.size()!=1 || numberedRows.rows[0][0].value!="two" || numberedRows.rows[0][1].value!="1" || numberedRows.rows[0][2].value!="1") return 85;
    for(const auto* sql:{"SELECT ?", "SELECT ?2", "SELECT ?01", "SELECT ?1, :alias", "SELECT :alias, ?1", "SELECT ?65"}) if(querySqliteSnapshotBound(snapshot,sql,numbered).error.empty()) return 86;
    std::vector<SqlParameter> jsonNumbered; std::string numberError;
    if(!parseSqlParametersJson(R"({"?1":{"type":"integer","value":"1"},"?2":{"type":"text","value":"two"}})",jsonNumbered,numberError) || jsonNumbered.size()!=2) return 87;
    if (querySqliteSnapshotBound(snapshot, "SELECT :id", {{"id", SqlValueType::Integer, 1, 0, {}}}).error.empty()) return 35;
    if (querySqliteSnapshotBound(snapshot, "SELECT :v", {{":v", SqlValueType::Float, 0, std::numeric_limits<double>::infinity(), {}}}).error.empty()) return 36;
    if (querySqliteSnapshotBound(snapshot, "SELECT :v", {{":v", SqlValueType::Text, 0, 0, std::string(256U * 1024U + 1, 'x')}}).error.empty()) return 37;
    if (querySqliteSnapshotBound(snapshot, "SELECT :x", {{":x", SqlValueType::Text, 0, 0, "1; DELETE FROM items"}}).rows[0][0].value != "1; DELETE FROM items") return 38;
    if (querySqliteSnapshotBound(snapshot, "SELECT :id", {id}, false, &cancelled).error.empty()) return 39;
    std::vector<SqlParameter> tooMany;
    for (int i = 0; i < 65; ++i) tooMany.push_back({":v" + std::to_string(i), SqlValueType::Integer, i, 0, {}});
    if (querySqliteSnapshotBound(snapshot, "SELECT 1", tooMany).error != "Too many SQL parameters") return 40;
    std::vector<SqlParameter> oversizedTotal;
    for (int i = 0; i < 5; ++i) oversizedTotal.push_back({":v" + std::to_string(i), SqlValueType::Blob, 0, 0, std::string(256U * 1024U, 'x')});
    if (querySqliteSnapshotBound(snapshot, "SELECT 1", oversizedTotal).error != "SQL parameter values exceed size limit") return 41;
    std::vector<SqlParameter> parsed; std::string parseError;
    if (!parseSqlParametersJson(R"({":id":{"type":"integer","value":"-9223372036854775808"},":real":{"type":"real","value":1.25},":text":{"type":"text","value":"a\u0000b"},":blob":{"type":"blob","value":"00Ff"},":null":{"type":"null"}})", parsed, parseError) ||
        parsed.size() != 5 || parsed[0].integer != std::numeric_limits<std::int64_t>::min() || parsed[2].bytes != std::string("a\0b", 3) || parsed[3].bytes != std::string("\0\xff", 2)) return 42;
    auto parsedQuery = querySqliteSnapshotBound(snapshot, "SELECT :id, :real, :text, :blob, :null", parsed);
    if (!parsedQuery.error.empty() || parsedQuery.rows.size() != 1 || parsedQuery.rows[0][0].value != "-9223372036854775808" || parsedQuery.rows[0][3].type != SqlValueType::Blob || parsedQuery.rows[0][4].type != SqlValueType::Null) return 43;
    for (const auto* invalid : {R"({":x":{"type":"integer","value":"9223372036854775808"}})", R"({":x":{"type":"integer","value":42}})", R"({":x":{"type":"blob","value":"0z"}})", R"({":x":{"type":"blob","value":"0"}})", R"({":x":{"type":"null","value":0}})", R"({":x":{"type":"text","value":"a","value":"b"}})", R"({":x":{"type":"text","value":"a","extra":0}})", R"({":x":{"type":"null"},":x":{"type":"null"}})", R"({"x":{"type":"null"}})"})
        if (parseSqlParametersJson(invalid, parsed, parseError) || !parsed.empty() || parseError.empty()) return 44;
    if (!parseSqlParametersJson(R"({":n":{"type":"null","value":null},":b":{"type":"blob","value":""}})", parsed, parseError) || parsed.size() != 2 || parsed[1].type != SqlValueType::Blob || !parsed[1].bytes.empty()) return 45;
    if (parseSqlParametersJson(std::string(2U * 1024U * 1024U + 1, ' '), parsed, parseError) || !parsed.empty()) return 46;
    // Table-valued pragma syntax must not bypass the read-only authorizer.
    if (querySqliteSnapshot(snapshot, "SELECT * FROM pragma_table_info('items')").error.empty()) return 47;
    // A rejected or interrupted result never leaves partially materialized rows.
    const auto interrupted = querySqliteSnapshot(snapshot, "SELECT * FROM items", false, &cancelled);
    if (interrupted.error.empty() || !interrupted.rows.empty() || !interrupted.columns.empty()) return 48;
    SqlQueryResult exportResult; exportResult.columns = {"=heading", "text", "blob", "null"};
    exportResult.rows = {{{SqlValueType::Text, "=1+1", false}, {SqlValueType::Text, "a,\"b\"\nline", false},
                          {SqlValueType::Blob, std::string("\0\xff", 2), false}, {SqlValueType::Null, {}, false}}};
    auto csv = serializeSqlResultCsv(exportResult);
    if (!csv.error.empty() || !csv.protectedFormulas || csv.partial || csv.text != "\"'=heading\",text,blob,null\r\n\"'=1+1\",\"a,\"\"b\"\"\nline\",0x00ff,\r\n") return 49;
    exportResult.rows[0][0] = {SqlValueType::Integer, "-123", false};
    csv = serializeSqlResultCsv(exportResult);
    if (!csv.error.empty() || csv.text.find("\r\n-123,") == std::string::npos) return 50;
    exportResult.rows[0][0] = {SqlValueType::Text, "  @sum", false};
    csv = serializeSqlResultCsv(exportResult);
    if (!csv.error.empty() || csv.text.find("\"'  @sum\"") == std::string::npos) return 51;
    exportResult.columns[0] = std::string("\xef\xbb\xbf", 3) + "=header";
    exportResult.rows[0][0] = {SqlValueType::Text, " \ttext", false};
    csv = serializeSqlResultCsv(exportResult);
    if (!csv.error.empty() || !csv.protectedFormulas || !csv.text.starts_with(std::string("\"'\xef\xbb\xbf", 5) + "=header\"") || csv.text.find("\"' \ttext\"") == std::string::npos) return 58;
    exportResult.truncated = true;
    csv = serializeSqlResultCsv(exportResult); if (csv.error.empty() || !csv.text.empty()) return 52;
    SqlCsvOptions allow; allow.allowPartial = true;
    csv = serializeSqlResultCsv(exportResult, allow); if (!csv.error.empty() || !csv.partial) return 53;
    exportResult.truncated = false; exportResult.rows[0][1].value = std::string("x\0y", 3);
    csv = serializeSqlResultCsv(exportResult); if (csv.error.empty() || !csv.text.empty()) return 54;
    exportResult.rows[0][1].value = std::string(1, static_cast<char>(0xff));
    csv = serializeSqlResultCsv(exportResult); if (csv.error.empty() || !csv.text.empty()) return 55;
    csv = serializeSqlResultCsv(exportResult, {}, &cancelled); if (csv.error != "SQL export cancelled" || !csv.text.empty()) return 56;
    namespace fs = std::filesystem;
    const auto exportRoot = fs::temp_directory_path() / ("lucid-sql-export-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(exportRoot);
    { std::ofstream out(exportRoot / "data.csv", std::ios::binary); out.write(snapshot.data(), static_cast<std::streamsize>(snapshot.size())); }
    bool projectCreated = false; std::string exportError;
    auto project = lucid::workspace::Project::openOrCreate(exportRoot, projectCreated, exportError, false, false);
    exportResult.rows[0][1].value = "text"; csv = serializeSqlResultCsv(exportResult);
    bool saved = exportError.empty() && csv.error.empty() &&
        !saveSqlResultCsv(project, "data.csv", "data.csv", csv, exportError) &&
        !saveSqlResultCsv(project, "data.csv", "../outside.csv", csv, exportError) &&
        !saveSqlResultCsv(project, "data.csv", "result.txt", csv, exportError) &&
        saveSqlResultCsv(project, "data.csv", "result.csv", csv, exportError);
    std::string savedBytes;
    saved = saved && project.readFile("result.csv", savedBytes, exportError) && savedBytes == csv.text &&
        !saveSqlResultCsv(project, "data.csv", "result.csv", csv, exportError);
    const std::u8string utf8Dir = u8"données/数据库.csv", utf8Output = u8"données/résultat.CSV";
    const std::string unicodeDatabase(utf8Dir.begin(), utf8Dir.end()), unicodeOutput(utf8Output.begin(), utf8Output.end());
    saved = saved && project.writeFile(unicodeDatabase, snapshot, exportError, nullptr, true) &&
        saveSqlResultCsv(project, unicodeDatabase, unicodeOutput, csv, exportError) &&
        project.readFile(unicodeOutput, savedBytes, exportError) && savedBytes == csv.text;
    const std::string wrong = "old content";
    saved = saved && !saveSqlResultCsv(project, "data.csv", "result.csv", csv, exportError, &wrong) &&
        project.readFile("result.csv", savedBytes, exportError) && savedBytes == csv.text;
    const auto expected = savedBytes; auto secondCsv = csv; secondCsv.text += "more\r\n";
    saved = saved && saveSqlResultCsv(project, "data.csv", "result.csv", secondCsv, exportError, &expected) &&
        project.readFile("result.csv", savedBytes, exportError) && savedBytes == secondCsv.text &&
        !saveSqlResultCsv(project, "data.csv", "cancel.csv", csv, exportError, nullptr, &cancelled) && !fs::exists(exportRoot / "cancel.csv");
    std::error_code linkError; fs::create_hard_link(exportRoot / "data.csv", exportRoot / "alias.csv", linkError);
    if (!linkError) saved = saved && !saveSqlResultCsv(project, "data.csv", "alias.csv", csv, exportError, &snapshot);
    fs::create_directory(exportRoot / "real-dir");
    std::error_code symlinkError; fs::create_directory_symlink(exportRoot / "real-dir", exportRoot / "linked-dir", symlinkError);
    if (!symlinkError) saved = saved && !saveSqlResultCsv(project, "data.csv", "linked-dir/result.csv", csv, exportError) && !fs::exists(exportRoot / "real-dir" / "result.csv");
    std::ifstream dbFile(exportRoot / "data.csv", std::ios::binary); const std::string afterDb((std::istreambuf_iterator<char>(dbFile)), {});
    saved = saved && afterDb == snapshot;
    fs::remove_all(exportRoot); if (!saved) return 57;
    return 0;
}
