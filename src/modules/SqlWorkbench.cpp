#include "modules/SqlWorkbench.hpp"
#include "editor/json/Json.hpp"
#include <sqlite3.h>
#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstring>
#include <map>
#include <memory>
#include <utility>
namespace lucid::modules {
SqlClassification classifySql(std::string_view sql) {
    if (sql.size() > 1024U * 1024U) return {SqlStatementKind::Unknown, "SQL exceeds size limit"};
    std::string first; bool ended = false, seen = false;
    for (std::size_t i = 0; i < sql.size();) {
        unsigned char c = static_cast<unsigned char>(sql[i]);
        if (std::isspace(c)) { ++i; continue; }
        if (c == '-' && i + 1 < sql.size() && sql[i+1] == '-') { i += 2; while (i < sql.size() && sql[i] != '\n' && sql[i] != '\r') ++i; continue; }
        if (c == '/' && i + 1 < sql.size() && sql[i+1] == '*') {
            auto end = sql.find("*/", i + 2); if (end == std::string_view::npos) return {SqlStatementKind::Unknown, "Unterminated comment"};
            if (sql.substr(i + 2, end - i - 2).find("/*") != std::string_view::npos) return {SqlStatementKind::Unknown, "Nested comments are unsupported"};
            i = end + 2; continue;
        }
        if (ended) return {SqlStatementKind::Unknown, "Multiple statements or trailing tokens"};
        if (c == ';') { if (!seen) return {SqlStatementKind::Unknown, "Empty statement prefix"}; ended = true; ++i; continue; }
        if (!seen) {
            if (!std::isalpha(c) || c >= 128) return {SqlStatementKind::Unknown, "Unsupported statement prefix"};
            while (i < sql.size()) { unsigned char ch = static_cast<unsigned char>(sql[i]); if (!std::isalpha(ch) || ch >= 128) break; first.push_back(static_cast<char>(std::toupper(ch))); ++i; }
            if (i < sql.size() && (std::isalnum(static_cast<unsigned char>(sql[i])) || sql[i] == '_' || static_cast<unsigned char>(sql[i]) >= 128)) return {SqlStatementKind::Unknown, "Unsupported statement prefix"};
            seen = true; continue;
        }
        if (c == '\'' || c == '"' || c == '`' || c == '[') {
            const char close = c == '[' ? ']' : static_cast<char>(c); bool closed = false; ++i;
            while (i < sql.size()) { if (sql[i] == close) { if (close != ']' && i + 1 < sql.size() && sql[i+1] == close) { i += 2; continue; } ++i; closed = true; break; } ++i; }
            if (!closed) return {SqlStatementKind::Unknown, "Unterminated quoted value"};
            continue;
        }
        // Accept only simple SQLite $name parameters. A second '$' still fails
        // closed, so PostgreSQL-style dollar quotes cannot hide a semicolon.
        if (c == '$') {
            const auto first = [](unsigned char ch) { return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_'; };
            const auto rest = [&](unsigned char ch) { return first(ch) || (ch >= '0' && ch <= '9'); };
            const auto start = ++i;
            if (i == sql.size() || !first(static_cast<unsigned char>(sql[i]))) return {SqlStatementKind::Unknown, "Unsupported SQL syntax"};
            while (i < sql.size() && rest(static_cast<unsigned char>(sql[i]))) ++i;
            if (i - start > 128) return {SqlStatementKind::Unknown, "Unsupported SQL syntax"};
            continue;
        }
        // Backslash escapes and NULs can hide statement boundaries.
        if (c == '\\' || c == 0 || c >= 128) return {SqlStatementKind::Unknown, "Unsupported SQL syntax"};
        ++i;
    }
    if (!seen) return {SqlStatementKind::Empty, "No statement"};
    if (first == "SELECT" || first == "VALUES") return {SqlStatementKind::Query, "Query-shaped statement; side effects are not ruled out"};
    if (first == "INSERT" || first == "UPDATE" || first == "DELETE" || first == "REPLACE") return {SqlStatementKind::DataChange, "Data-changing statement"};
    if (first == "CREATE" || first == "ALTER" || first == "DROP" || first == "VACUUM" || first == "REINDEX") return {SqlStatementKind::SchemaChange, "Schema or database maintenance statement"};
    if (first == "BEGIN" || first == "COMMIT" || first == "ROLLBACK" || first == "SAVEPOINT" || first == "RELEASE" || first == "END") return {SqlStatementKind::Transaction, "Transaction-control statement"};
    return {SqlStatementKind::Unknown, "Statement form requires dialect-aware analysis"};
}

namespace {
constexpr std::size_t kMaxDbBytes = 64U * 1024U * 1024U;
constexpr std::size_t kMaxSqlBytes = 1024U * 1024U;
constexpr std::size_t kMaxRows = 500;
constexpr std::size_t kMaxCellBytes = 16U * 1024U;
constexpr std::size_t kMaxResultBytes = 4U * 1024U * 1024U;
constexpr std::size_t kMaxParameters = 64, kMaxParameterBytes = 256U * 1024U, kMaxTotalParameterBytes = 1024U * 1024U;

bool validParameterName(std::string_view name) {
    if (name.size() < 2 || name.size() > 129 || (name[0] != ':' && name[0] != '@' && name[0] != '$')) return false;
    const auto first = [](unsigned char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; };
    const auto rest = [&](unsigned char c) { return first(c) || (c >= '0' && c <= '9'); };
    return first(static_cast<unsigned char>(name[1])) && std::all_of(name.begin() + 2, name.end(), rest);
}

int readOnlyAuthorizer(void* context, int action, const char* first, const char*, const char*, const char*) {
    switch (action) {
        case SQLITE_SELECT: case SQLITE_READ: case SQLITE_FUNCTION: case SQLITE_RECURSIVE:
            return SQLITE_OK;
        case SQLITE_PRAGMA:
            return context && *static_cast<const bool*>(context) && first && std::strcmp(first, "table_xinfo") == 0 ? SQLITE_OK : SQLITE_DENY;
        default: return SQLITE_DENY;
    }
}
struct QueryBudget { const std::atomic_bool* cancel; unsigned callbacks = 0; };
int queryProgress(void* context) {
    auto& budget = *static_cast<QueryBudget*>(context);
    return (++budget.callbacks > 3000 || (budget.cancel && budget.cancel->load())) ? 1 : 0;
}
using Database = std::unique_ptr<sqlite3, decltype(&sqlite3_close)>;
using Statement = std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>;
}

bool parseSqlParametersJson(std::string_view json, std::vector<SqlParameter>& parameters, std::string& error) {
    parameters.clear(); error.clear();
    if (json.size() > 2U * 1024U * 1024U) { error = "SQL parameter JSON exceeds size limit"; return false; }
    editor::JsonError parseError; auto root = editor::parseJson(json, parseError);
    if (!root || root->type != editor::JsonType::Object || root->children.size() > kMaxParameters) { error = "Expected a bounded SQL parameter JSON object"; return false; }
    std::vector<SqlParameter> result; std::map<std::string, bool> names; std::size_t totalBytes = 0;
    const auto fail = [&error](const char* message) { error = message; return false; };
    for (const auto& item : root->children) {
        if (!validParameterName(item.key) || !names.emplace(item.key, true).second || item.type != editor::JsonType::Object)
            return fail("Invalid or duplicate SQL parameter name");
        const editor::JsonValue* type = nullptr; const editor::JsonValue* value = nullptr;
        for (const auto& field : item.children) {
            if (field.key == "type" && !type) type = &field;
            else if (field.key == "value" && !value) value = &field;
            else return fail("Unknown or duplicate SQL parameter field");
        }
        if (!type || type->type != editor::JsonType::String) return fail("SQL parameter type is required");
        SqlParameter parameter; parameter.name = item.key;
        if (type->stringValue == "null") {
            if (value && value->type != editor::JsonType::Null) return fail("Null SQL parameter must have a null value");
        } else if (type->stringValue == "integer") {
            if (!value || value->type != editor::JsonType::String || value->stringValue.empty()) return fail("Integer SQL parameter must be a decimal string");
            const auto& text = value->stringValue; const auto parsed = std::from_chars(text.data(), text.data() + text.size(), parameter.integer);
            if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) return fail("Integer SQL parameter is invalid or out of range");
            parameter.type = SqlValueType::Integer;
        } else if (type->stringValue == "real") {
            if (!value || value->type != editor::JsonType::Number || !std::isfinite(value->numberValue)) return fail("Real SQL parameter must be finite");
            parameter.type = SqlValueType::Float; parameter.real = value->numberValue;
        } else if (type->stringValue == "text" || type->stringValue == "blob") {
            if (!value || value->type != editor::JsonType::String) return fail("Text and blob SQL parameters require a string value");
            const auto& bytes = value->stringValue;
            if (type->stringValue == "text") { parameter.type = SqlValueType::Text; parameter.bytes = bytes; }
            else {
                if (bytes.size() % 2 || bytes.size() / 2 > kMaxParameterBytes) return fail("Blob SQL parameter must be bounded even-length hexadecimal");
                parameter.type = SqlValueType::Blob; parameter.bytes.reserve(bytes.size() / 2);
                const auto digit = [](unsigned char c) -> int { if (c >= '0' && c <= '9') return c - '0'; if (c >= 'a' && c <= 'f') return c - 'a' + 10; if (c >= 'A' && c <= 'F') return c - 'A' + 10; return -1; };
                for (std::size_t i = 0; i < bytes.size(); i += 2) {
                    const int high = digit(static_cast<unsigned char>(bytes[i])), low = digit(static_cast<unsigned char>(bytes[i + 1]));
                    if (high < 0 || low < 0) return fail("Blob SQL parameter contains invalid hexadecimal");
                    parameter.bytes.push_back(static_cast<char>((high << 4) | low));
                }
            }
            if (parameter.bytes.size() > kMaxParameterBytes || parameter.bytes.size() > kMaxTotalParameterBytes - totalBytes) return fail("SQL parameter values exceed size limit");
            totalBytes += parameter.bytes.size();
        } else return fail("Unsupported SQL parameter type");
        result.push_back(std::move(parameter));
    }
    parameters = std::move(result); return true;
}

SqlQueryResult querySqliteSnapshot(std::string_view database, std::string_view sql,
                                  bool explain, const std::atomic_bool* cancel) {
    return querySqliteSnapshotBound(database, sql, {}, explain, cancel);
}

static SqlQueryResult querySqliteSnapshotImpl(std::string_view database, std::string_view sql,
    const std::vector<SqlParameter>& parameters, bool explain, const std::atomic_bool* cancel, bool allowColumnMetadata) {
    SqlQueryResult result;
    const auto fail = [&](std::string message) { result.error = std::move(message); result.rows.clear(); result.columns.clear(); return result; };
    if (cancel && cancel->load()) return fail("Query cancelled");
    if (database.size() < 100 || database.size() > kMaxDbBytes || database.substr(0, 16) != std::string_view("SQLite format 3\0", 16))
        return fail("Expected a SQLite database snapshot of at most 64 MiB");
    if (static_cast<unsigned char>(database[18]) != 1 || static_cast<unsigned char>(database[19]) != 1)
        return fail("WAL-format databases are unsupported; export a standalone snapshot first");
    if (sql.empty() || sql.size() > kMaxSqlBytes || sql.find('\0') != std::string_view::npos)
        return fail("SQL must be nonempty, contain no NUL, and be at most 1 MiB");
    if (parameters.size() > kMaxParameters) return fail("Too many SQL parameters");
    std::map<std::string, const SqlParameter*> bindings;
    std::size_t totalParameterBytes = 0;
    for (const auto& parameter : parameters) {
        if (!validParameterName(parameter.name) || !bindings.emplace(parameter.name, &parameter).second)
            return fail("SQL parameter names must be unique named identifiers");
        switch (parameter.type) {
            case SqlValueType::Null: case SqlValueType::Integer: break;
            case SqlValueType::Float:
                if (!std::isfinite(parameter.real)) return fail("SQL floating-point parameter must be finite");
                break;
            case SqlValueType::Text: case SqlValueType::Blob:
                if (parameter.bytes.size() > kMaxParameterBytes || parameter.bytes.size() > kMaxTotalParameterBytes - totalParameterBytes)
                    return fail("SQL parameter values exceed size limit");
                totalParameterBytes += parameter.bytes.size(); break;
            default: return fail("Unsupported SQL parameter type");
        }
    }
    sqlite3* raw = nullptr;
    const int opened = sqlite3_open_v2(":memory:", &raw, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_MEMORY, nullptr);
    Database db(raw, sqlite3_close);
    if (opened != SQLITE_OK) return fail("Could not create SQLite snapshot connection");
    sqlite3_limit(db.get(), SQLITE_LIMIT_LENGTH, 1024 * 1024);
    sqlite3_limit(db.get(), SQLITE_LIMIT_SQL_LENGTH, static_cast<int>(kMaxSqlBytes + 32));
    sqlite3_limit(db.get(), SQLITE_LIMIT_COLUMN, 128);
    sqlite3_limit(db.get(), SQLITE_LIMIT_EXPR_DEPTH, 200);
    sqlite3_limit(db.get(), SQLITE_LIMIT_COMPOUND_SELECT, 100);
    sqlite3_limit(db.get(), SQLITE_LIMIT_VARIABLE_NUMBER, static_cast<int>(kMaxParameters));
    // Fail closed if the SQLite build cannot enforce the restrictions expected
    // for untrusted project snapshots. The authorizer remains the primary rule
    // that limits queries to reads; defensive mode is an additional safeguard.
    if (sqlite3_db_config(db.get(), SQLITE_DBCONFIG_TRUSTED_SCHEMA, 0, nullptr) != SQLITE_OK ||
        sqlite3_db_config(db.get(), SQLITE_DBCONFIG_DEFENSIVE, 1, nullptr) != SQLITE_OK)
        return fail("Could not secure SQLite snapshot connection");
    auto* buffer = static_cast<unsigned char*>(sqlite3_malloc64(database.size()));
    if (!buffer) return fail("Could not allocate database snapshot");
    std::memcpy(buffer, database.data(), database.size());
    // FREEONCLOSE transfers ownership even when sqlite3_deserialize fails.
    const int loaded = sqlite3_deserialize(db.get(), "main", buffer,
        static_cast<sqlite3_int64>(database.size()), static_cast<sqlite3_int64>(database.size()),
        SQLITE_DESERIALIZE_FREEONCLOSE | SQLITE_DESERIALIZE_READONLY);
    if (loaded != SQLITE_OK) return fail("Could not load SQLite snapshot");
    if (cancel && cancel->load()) return fail("Query cancelled");
    if (sqlite3_set_authorizer(db.get(), readOnlyAuthorizer, &allowColumnMetadata) != SQLITE_OK)
        return fail("Could not restrict SQLite snapshot connection");
    QueryBudget budget{cancel}; sqlite3_progress_handler(db.get(), 1000, queryProgress, &budget);
    const std::string query = (explain ? "EXPLAIN QUERY PLAN " : "") + std::string(sql);
    sqlite3_stmt* rawStatement = nullptr; const char* tail = nullptr;
    const int prepared = sqlite3_prepare_v3(db.get(), query.c_str(), static_cast<int>(query.size()), 0, &rawStatement, &tail);
    Statement statement(rawStatement, sqlite3_finalize);
    if (cancel && cancel->load()) return fail("Query cancelled");
    if (prepared != SQLITE_OK) return fail(std::string("SQLite: ") + sqlite3_errmsg(db.get()));
    if (!statement || !sqlite3_stmt_readonly(statement.get()) || sqlite3_column_count(statement.get()) == 0)
        return fail("Only a single read-only query is supported");
    sqlite3_stmt* rawExtra = nullptr;
    const int extraStatus = sqlite3_prepare_v3(db.get(), tail, -1, 0, &rawExtra, nullptr);
    Statement extra(rawExtra, sqlite3_finalize);
    if (extraStatus != SQLITE_OK || extra) return fail("Only one SQL statement is supported");
    const int variableCount = sqlite3_bind_parameter_count(statement.get());
    for (int i = 1; i <= variableCount; ++i) {
        const char* rawName = sqlite3_bind_parameter_name(statement.get(), i);
        if (!rawName || !validParameterName(rawName)) return fail("Only named SQL parameters are supported");
        auto found = bindings.find(rawName);
        if (found == bindings.end()) return fail("Missing SQL parameter binding");
        const auto& parameter = *found->second;
        int status = SQLITE_MISUSE;
        switch (parameter.type) {
            case SqlValueType::Null: status = sqlite3_bind_null(statement.get(), i); break;
            case SqlValueType::Integer: status = sqlite3_bind_int64(statement.get(), i, parameter.integer); break;
            case SqlValueType::Float: status = sqlite3_bind_double(statement.get(), i, parameter.real); break;
            case SqlValueType::Text: status = sqlite3_bind_text64(statement.get(), i, parameter.bytes.data(), parameter.bytes.size(), SQLITE_TRANSIENT, SQLITE_UTF8); break;
            case SqlValueType::Blob: status = sqlite3_bind_blob64(statement.get(), i, parameter.bytes.empty() ? "" : parameter.bytes.data(), parameter.bytes.size(), SQLITE_TRANSIENT); break;
        }
        if (status != SQLITE_OK) return fail("Could not bind SQL parameter");
        bindings.erase(found);
    }
    if (!bindings.empty()) return fail("Unused SQL parameter binding");
    const int columns = sqlite3_column_count(statement.get());
    for (int i = 0; i < columns; ++i) {
        const char* name = sqlite3_column_name(statement.get(), i);
        if (!name) return fail("Could not read SQLite column name");
        result.columns.emplace_back(name);
    }
    std::size_t resultBytes = 0;
    for (;;) {
        if (cancel && cancel->load()) return fail("Query cancelled");
        const int status = sqlite3_step(statement.get());
        if (status == SQLITE_DONE) break;
        if (status != SQLITE_ROW) return fail(status == SQLITE_INTERRUPT ? "Query cancelled or instruction limit reached" : std::string("SQLite: ") + sqlite3_errmsg(db.get()));
        if (result.rows.size() == kMaxRows) { result.truncated = true; break; }
        std::vector<SqlValue> row; row.reserve(static_cast<std::size_t>(columns));
        for (int i = 0; i < columns; ++i) {
            if (cancel && cancel->load()) return fail("Query cancelled");
            const int type = sqlite3_column_type(statement.get(), i);
            SqlValue value;
            if (type == SQLITE_NULL) value.type = SqlValueType::Null;
            else {
                value.type = type == SQLITE_INTEGER ? SqlValueType::Integer : type == SQLITE_FLOAT ? SqlValueType::Float : type == SQLITE_BLOB ? SqlValueType::Blob : SqlValueType::Text;
                const void* data = type == SQLITE_BLOB ? sqlite3_column_blob(statement.get(), i) : sqlite3_column_text(statement.get(), i);
                const auto size = static_cast<std::size_t>(sqlite3_column_bytes(statement.get(), i));
                const auto available = kMaxResultBytes - resultBytes;
                const auto copied = std::min({size, kMaxCellBytes, available});
                if (copied && data) value.value.assign(static_cast<const char*>(data), copied);
                value.truncated = copied < size; result.truncated = result.truncated || value.truncated;
                resultBytes += copied;
            }
            row.push_back(std::move(value));
        }
        result.rows.push_back(std::move(row));
        if (resultBytes == kMaxResultBytes) { result.truncated = true; break; }
    }
    return result;
}

SqlQueryResult querySqliteSnapshotBound(std::string_view database, std::string_view sql,
    const std::vector<SqlParameter>& parameters, bool explain, const std::atomic_bool* cancel) {
    return querySqliteSnapshotImpl(database, sql, parameters, explain, cancel, false);
}
SqlSchemaResult inspectSqliteSchema(std::string_view database, const std::atomic_bool* cancel) {
    SqlSchemaResult output;
    auto result=querySqliteSnapshot(database,
        "SELECT type, name, tbl_name, sql FROM sqlite_schema "
        "WHERE type IN ('table','view','index','trigger') AND substr(name,1,7) <> 'sqlite_' ORDER BY type, name",false,cancel);
    if (!result.error.empty()) { output.error=std::move(result.error); return output; }
    output.truncated=result.truncated;
    for (const auto& row : result.rows) {
        if (cancel && cancel->load()) { output={}; output.error="Schema inspection cancelled"; return output; }
        if (row.size()!=4 || row[0].type!=SqlValueType::Text || row[1].type!=SqlValueType::Text ||
            row[2].type!=SqlValueType::Text || (row[3].type!=SqlValueType::Text && row[3].type!=SqlValueType::Null)) {
            output={}; output.error="Invalid SQLite schema metadata"; return output;
        }
        SqlSchemaObject object; object.type=row[0].value; object.name=row[1].value; object.table=row[2].value;
        object.definitionAvailable=row[3].type==SqlValueType::Text; if (object.definitionAvailable) object.definition=row[3].value;
        object.truncated=row[0].truncated || row[1].truncated || row[2].truncated || row[3].truncated;
        output.objects.push_back(std::move(object));
    }
    return output;
}

SqlColumnsResult inspectSqliteColumns(std::string_view database, std::string_view table, const std::atomic_bool* cancel) {
    SqlColumnsResult output;
    if (table.empty() || table.size() > 1024 || table.find('\0') != std::string_view::npos) { output.error = "Invalid table name"; return output; }
    const std::vector<SqlParameter> params{{":table", SqlValueType::Text, 0, 0, std::string(table)}};
    auto result = querySqliteSnapshotImpl(database,
        "SELECT name, type, \"notnull\", dflt_value, pk, hidden FROM pragma_table_xinfo(:table) ORDER BY cid",
        params, false, cancel, true);
    if (!result.error.empty()) { output.error = std::move(result.error); return output; }
    output.truncated = result.truncated;
    const auto number = [](const SqlValue& value, int& out) {
        if (value.type != SqlValueType::Integer || value.truncated) return false;
        auto parsed = std::from_chars(value.value.data(), value.value.data() + value.value.size(), out);
        return parsed.ec == std::errc{} && parsed.ptr == value.value.data() + value.value.size();
    };
    for (const auto& row : result.rows) {
        int notNull = 0, primaryKey = 0, hidden = 0;
        if (row.size() != 6 || row[0].type != SqlValueType::Text || row[1].type != SqlValueType::Text ||
            !number(row[2], notNull) || (row[3].type != SqlValueType::Text && row[3].type != SqlValueType::Null) ||
            !number(row[4], primaryKey) || !number(row[5], hidden) || notNull < 0 || notNull > 1 || primaryKey < 0 || hidden < 0 || hidden > 3) {
            output = {}; output.error = "Invalid SQLite column metadata"; return output;
        }
        SqlColumn column; column.name = row[0].value; column.declaredType = row[1].value; column.notNull = notNull != 0;
        column.hasDefault = row[3].type == SqlValueType::Text; if (column.hasDefault) column.defaultValue = row[3].value;
        column.primaryKeyOrder = primaryKey; column.hiddenKind = hidden;
        column.truncated = row[0].truncated || row[1].truncated || row[3].truncated;
        output.truncated = output.truncated || column.truncated;
        output.columns.push_back(std::move(column));
    }
    if (output.columns.empty() && !output.truncated) output.error = "Table or view was not found";
    return output;
}

}
