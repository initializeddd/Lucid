#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace lucid::modules {
struct LogRecord { std::string source, text; std::size_t line = 0; std::int64_t timestampMillis = 0; };
struct LogInput { std::string source; std::string_view text; };
struct LogCorrelationOptions { int syslogYear = 0; int syslogUtcOffsetMinutes = 0; bool enableSyslog = false; bool enableJsonLines = false; };
struct LogCorrelationResult { std::vector<LogRecord> records; std::size_t unparseableLines = 0, scannedLines = 0, jsonLines = 0; bool incomplete = false; };
// ISO 8601 dates with explicit Z or numeric offset, and optionally syslog with
// caller-supplied year and UTC offset. Optional JSON Lines require one top-level
// string field named timestamp, @timestamp, or time with an explicit ISO zone;
// duplicate/ambiguous timestamp fields and lines over 64 KiB are rejected.
// JSON values are not redacted: logs may contain secrets. Bounded by input bytes,
// scanned lines and result count. If the result limit is reached, the earliest
// matching records are retained. Matching is case-sensitive and searches text
// and source. Cancellation marks the result incomplete.
[[nodiscard]] LogCorrelationResult correlateLogs(const std::vector<LogInput>& inputs, std::string_view query = {},
    std::size_t maxBytes = 16 * 1024 * 1024, std::size_t maxLines = 100000, std::size_t maxRecords = 10000,
    LogCorrelationOptions options = {}, const std::atomic_bool* cancel = nullptr);
}
