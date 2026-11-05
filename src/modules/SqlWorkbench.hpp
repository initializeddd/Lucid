#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
namespace lucid::workspace { class Project; }
namespace lucid::modules {
enum class SqlStatementKind { Empty, Query, DataChange, SchemaChange, Transaction, Unknown };
struct SqlClassification {
    SqlStatementKind kind = SqlStatementKind::Unknown;
    std::string reason;
};
// Lexical classification of one SQL statement, not validation or permission
// to execute. Query can call functions with side effects; syntax, dialect,
// privileges, and actual read-only behavior cannot be inferred here.
[[nodiscard]] SqlClassification classifySql(std::string_view sql);

enum class SqlValueType { Null, Integer, Float, Text, Blob };
struct SqlValue { SqlValueType type = SqlValueType::Null; std::string value; bool truncated = false; };
struct SqlQueryResult {
    std::vector<std::string> columns;
    std::vector<std::vector<SqlValue>> rows;
    std::string error;
    bool truncated = false;
};
struct SqlCsvOptions {
    bool includeHeader = true;
    bool protectSpreadsheetFormulas = true;
    // Partial output is rejected unless explicitly selected by the caller.
    bool allowPartial = false;
};
struct SqlCsvExport {
    std::string text, error;
    bool partial = false, protectedFormulas = false;
};
// Serializes bounded query results as UTF-8 CSV without reading/writing files.
// NULL is an unquoted empty field; empty text is quoted. BLOBs become `0x`
// followed by lowercase hex, which may be indistinguishable from text in CSV.
// Spreadsheet formula protection prepends an apostrophe to risky text/header
// fields, changing those field values. This is best-effort: spreadsheet import
// behavior varies, and editing or resaving can remove protection. CSV readers may not preserve the quoted
// distinction between NULL and empty text. Results can contain secrets.
[[nodiscard]] SqlCsvExport serializeSqlResultCsv(const SqlQueryResult& result,
    const SqlCsvOptions& options = {}, const std::atomic_bool* cancel = nullptr);
// Explicitly saves a previously reviewed export to a project-relative .csv.
// The database path is checked against the destination, including existing
// hard-link aliases; destination symlink/reparse-point components are rejected.
// A null expected pointer means create-only; otherwise the
// destination must still match those exact bytes. Project file checks cannot
// eliminate races with another process between final checks and replacement.
[[nodiscard]] bool saveSqlResultCsv(workspace::Project& project, std::string_view databasePath,
    std::string_view destination, const SqlCsvExport& csv, std::string& error,
    const std::string* expected = nullptr, const std::atomic_bool* cancel = nullptr);
// Parameter names are :name, @name, $name or explicitly numbered ?1.. ?64.
// Text and blob values may contain NUL bytes. No SQL interpolation
// is performed; values are passed to SQLite through its binding API.
struct SqlParameter {
    std::string name;
    SqlValueType type = SqlValueType::Null;
    std::int64_t integer = 0;
    double real = 0;
    std::string bytes;
};
// Parse a bounded JSON object mapping SQLite names to typed values, e.g.
// {":id":{"type":"integer","value":"42"},":blob":{"type":"blob","value":"00ff"}}.
// Integers are exact signed decimal strings; reals are JSON numbers, text and
// hex-encoded blobs are strings, and null accepts an absent or null value.
// Unknown/duplicate fields and malformed names or values are rejected.
[[nodiscard]] bool parseSqlParametersJson(std::string_view json, std::vector<SqlParameter>& parameters, std::string& error);
// Execute exactly one read-only SQLite query against a bounded in-memory copy.
// WAL-format files are rejected because their companion journal is not captured.
// No external files, extensions, or database writes are permitted. Result sizes,
// SQLite VM instructions and input sizes are bounded. If explain is true the
// output is SQLite's EXPLAIN QUERY PLAN rather than the query's result rows.
[[nodiscard]] SqlQueryResult querySqliteSnapshot(std::string_view database, std::string_view sql,
                                               bool explain = false, const std::atomic_bool* cancel = nullptr);
// Every statement parameter must be supplied exactly once; anonymous, mixed named
// and numbered, sparse numbered, duplicate and missing bindings are rejected. At most 64 values,
// 256 KiB per text/blob and 1 MiB total payload are accepted.
[[nodiscard]] SqlQueryResult querySqliteSnapshotBound(std::string_view database, std::string_view sql,
    const std::vector<SqlParameter>& parameters, bool explain = false, const std::atomic_bool* cancel = nullptr);
// Internal safety helpers shared by query and change-preview paths.
[[nodiscard]] bool validSqlParameterName(std::string_view name);
[[nodiscard]] bool safeSqlParameterSyntax(std::string_view sql);
struct SqlSchemaObject {
    std::string type, name, table, definition;
    bool definitionAvailable = false, truncated = false;
};
struct SqlSchemaResult { std::vector<SqlSchemaObject> objects; std::string error; bool truncated = false; };
// Enumerates user tables, views, indexes and triggers from sqlite_schema in the
// same bounded read-only snapshot as queries. It does not execute stored SQL.
// Definitions may be NULL. Truncated names/definitions
// must not be used as complete SQL identifiers or statements.
[[nodiscard]] SqlSchemaResult inspectSqliteSchema(std::string_view database, const std::atomic_bool* cancel = nullptr);

struct SqlColumn {
    std::string name, declaredType, defaultValue;
    bool notNull = false, hasDefault = false, truncated = false;
    int primaryKeyOrder = 0, hiddenKind = 0;
};
struct SqlColumnsResult { std::vector<SqlColumn> columns; std::string error; bool truncated = false; };
// Inspects SQLite table/view columns in the same isolated snapshot. Hidden kind
// follows SQLite table_xinfo: 0 normal, 1 hidden, 2/3 generated. Truncated names
// and definitions must not be used as SQL identifiers or statements.
[[nodiscard]] SqlColumnsResult inspectSqliteColumns(std::string_view database, std::string_view table,
    const std::atomic_bool* cancel = nullptr);

}
