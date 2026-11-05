#pragma once
#include "modules/SqlWorkbench.hpp"
#include <atomic>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
namespace lucid::workspace { class Project; }
namespace lucid::modules {
// Preview is an isolated database copy, never a live database transaction.
// No SQL text or database contents are sent over the network.
enum class SqlRowOperation { Insert, Update, Delete };
struct SqlRowChange { SqlRowOperation operation; std::string table; std::int64_t rowid = 0; };
struct SqlChangePreview {
    std::string original, changed, sql, error;
    std::int64_t affected = 0;
    // SQLite update-hook observations, not a complete diff: changes to
    // WITHOUT ROWID tables, truncate-optimized deletes, and REPLACE conflict deletions can be omitted.
    // Values are never collected. At most 200 events are retained.
    std::vector<SqlRowChange> observedRows;
    bool observedRowsTruncated = false;
};
// Supports one INSERT/UPDATE/DELETE/REPLACE without RETURNING,
// triggers or schema changes. Bounded SQLite VM execution; no external files.
[[nodiscard]] SqlChangePreview previewSqliteChange(std::string_view database, std::string_view sql,
    const std::atomic_bool* cancel = nullptr);
// Named :name, @name, $name or contiguous numbered ?1.. ?64 SQLite bindings
// must all be supplied exactly once. Mixed, anonymous, missing, duplicate and unused parameters are rejected. Values
// are never interpolated into SQL or included in errors; bounded to 64 bindings,
// 256 KiB each and 1 MiB in total. The lexical policy currently rejects $name.
[[nodiscard]] SqlChangePreview previewSqliteChangeBound(std::string_view database, std::string_view sql,
    const std::vector<SqlParameter>& parameters, const std::atomic_bool* cancel = nullptr);
// Only creates a new .db file after explicit confirmation and an exact source
// snapshot recheck. Does not overwrite a database or its journal. The project
// filesystem API cannot exclude races with external processes changing paths or
// source content after recheck; do not use against active/live databases.
[[nodiscard]] bool exportSqliteChange(workspace::Project& project, std::string_view source,
    std::string_view destination, const SqlChangePreview& preview, bool confirmed,
    std::string& error, const std::atomic_bool* cancel = nullptr);
}
