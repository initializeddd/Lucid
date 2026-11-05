#pragma once
#include "modules/SqlChanges.hpp"
#include <atomic>
#include <string>
#include <vector>
namespace lucid::modules {
struct SqlRowCellDiff { std::string column; SqlValue before, after; bool changed=false; };
struct SqlRowDiff {
    std::vector<SqlRowCellDiff> cells;
    bool beforeExists=false, afterExists=false, truncated=false;
    std::string error;
};
// On-demand net comparison of a single observed row against the two isolated
// snapshots. It does not reconstruct intermediate changes. Row ID changes or
// REPLACE may look like insertion/deletion and SQLite omits some operations. Values can contain
// secrets. Only ordinary rowid tables are supported; an unshadowed SQLite rowid
// alias is required, and at most 64 columns and 1 KiB per cell are returned.
// SQLite result limits can truncate values; consumers must display that fact
// and must not treat an equal truncated prefix as proof of equality.
[[nodiscard]] SqlRowDiff inspectSqlChangeRow(const SqlChangePreview& preview,
    std::size_t observedIndex, const std::atomic_bool* cancel=nullptr);
}
