#include "modules/SqlWorkbench.hpp"
#include "text/Utf8.hpp"
#include "workspace/Project.hpp"
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <exception>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace lucid::modules {
namespace {
constexpr std::size_t maxOutput = 16U * 1024U * 1024U, maxRows = 500, maxColumns = 128, maxCell = 16384, maxHeader = 65536;
bool validText(std::string_view value) {
    for (std::size_t i = 0; i < value.size();) {
        const auto start = i; const auto c = text::nextUtf8(value, i);
        if ((c < 0x20 && c != U'\t' && c != U'\n' && c != U'\r') || c == 0x7f ||
            (c == U'\ufffd' && i == start + 1 && static_cast<unsigned char>(value[start]) >= 0x80U)) return false;
    }
    return true;
}
bool risky(std::string_view value) {
    // A leading tab or line break can itself be interpreted by spreadsheets;
    // also inspect the first character after whitespace.
    std::size_t i = 0;
    while (i < value.size() && (value[i] == ' ' || value[i] == '\t' || value[i] == '\r' || value[i] == '\n')) {
        if (value[i] != ' ') return true;
        ++i;
    }
    // A leading UTF-8 BOM can be stripped by an importer, exposing a formula.
    if (value.substr(i, 3) == std::string_view("\xef\xbb\xbf", 3)) return true;
    return i < value.size() && (value[i] == '=' || value[i] == '+' || value[i] == '-' || value[i] == '@');
}
bool noSymlinkDestination(const std::filesystem::path& root, const std::filesystem::path& relative) {
    const auto normalized = relative.lexically_normal();
    if (normalized.empty() || normalized == ".") return false;
    auto current = root;
    for (const auto& component : normalized) {
        if (component == "..") return false;
        if (component == ".") continue;
        current /= component;
        std::error_code ec; const auto status = std::filesystem::symlink_status(current, ec);
        if (ec == std::errc::no_such_file_or_directory || (!ec && status.type() == std::filesystem::file_type::not_found)) break;
        if (ec || status.type() == std::filesystem::file_type::symlink) return false;
#if defined(_WIN32)
        const auto attributes = GetFileAttributesW(current.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_REPARSE_POINT)) return false;
#endif
    }
    return true;
}
bool validNumber(std::string_view value, bool integer) {
    if (value.empty() || value.size() > 128) return false;
    if (!integer && (value == "Inf" || value == "-Inf")) return true;
    std::size_t i = value.front() == '-' ? 1 : 0, digits = i;
    while (i < value.size() && value[i] >= '0' && value[i] <= '9') ++i;
    if (i == digits) return false;
    if (!integer && i < value.size() && value[i] == '.') {
        ++i; const auto start = i;
        while (i < value.size() && value[i] >= '0' && value[i] <= '9') ++i;
        if (start == i) return false;
    }
    if (!integer && i < value.size() && (value[i] == 'e' || value[i] == 'E')) {
        ++i; if (i < value.size() && (value[i] == '+' || value[i] == '-')) ++i;
        const auto start = i; while (i < value.size() && value[i] >= '0' && value[i] <= '9') ++i;
        if (start == i) return false;
    }
    return i == value.size();
}
}
SqlCsvExport serializeSqlResultCsv(const SqlQueryResult& result, const SqlCsvOptions& options, const std::atomic_bool* cancel) {
    SqlCsvExport output;
    const auto fail = [&](std::string message) { output.text.clear(); output.error = std::move(message); return output; };
    if (cancel && cancel->load()) return fail("SQL export cancelled");
    if (!result.error.empty() || result.columns.empty() || result.columns.size() > maxColumns || result.rows.size() > maxRows)
        return fail("SQL result is invalid or exceeds export limits");
    output.partial = result.truncated;
    for (const auto& row : result.rows) {
        if (row.size() != result.columns.size()) return fail("SQL result row has an invalid column count");
        for (const auto& cell : row) if (cell.truncated) output.partial = true;
    }
    if (output.partial && !options.allowPartial) return fail("SQL result is incomplete; explicitly allow partial export to continue");
    auto append = [&](std::string_view value) {
        if (value.size() > maxOutput - output.text.size()) return false;
        output.text.append(value); return true;
    };
    auto field = [&](std::string_view value, bool textValue, bool forceQuotes) {
        if (!validText(value)) return false;
        const bool protect = options.protectSpreadsheetFormulas && textValue && risky(value);
        if (protect) output.protectedFormulas = true;
        const bool quote = forceQuotes || protect || value.find_first_of(",\"\r\n") != std::string_view::npos;
        if (quote && !append("\"")) return false;
        if (protect && !append("'")) return false;
        for (const char c : value) {
            if (c == '"' && !append("\"")) return false;
            if (!append(std::string_view(&c, 1))) return false;
        }
        return !quote || append("\"");
    };
    if (options.includeHeader) {
        for (std::size_t i = 0; i < result.columns.size(); ++i) {
            if ((cancel && cancel->load()) || result.columns[i].size() > maxHeader || (i && !append(",")) || !field(result.columns[i], true, false))
                return fail(cancel && cancel->load() ? "SQL export cancelled" : "Invalid or oversized SQL result header");
        }
        if (!append("\r\n")) return fail("SQL export exceeds size limit");
    }
    constexpr char hex[] = "0123456789abcdef";
    for (const auto& row : result.rows) {
        if (cancel && cancel->load()) return fail("SQL export cancelled");
        for (std::size_t i = 0; i < row.size(); ++i) {
            if (i && !append(",")) return fail("SQL export exceeds size limit");
            const auto& cell = row[i];
            if (cell.value.size() > maxCell) return fail("SQL result cell exceeds size limit");
            switch (cell.type) {
            case SqlValueType::Null:
                if (!cell.value.empty()) return fail("Invalid SQL NULL cell");
                break;
            case SqlValueType::Integer:
            case SqlValueType::Float:
                if (!validNumber(cell.value, cell.type == SqlValueType::Integer) || !field(cell.value, cell.value == "-Inf", false)) return fail("Invalid SQL numeric cell");
                break;
            case SqlValueType::Text:
                if (!field(cell.value, true, cell.value.empty())) return fail("Invalid or oversized SQL text cell");
                break;
            case SqlValueType::Blob: {
                if (!append("0x")) return fail("SQL export exceeds size limit");
                for (const unsigned char byte : cell.value) { const char pair[] = {hex[byte >> 4], hex[byte & 15]}; if (!append(std::string_view(pair, 2))) return fail("SQL export exceeds size limit"); }
                break;
            }
            default: return fail("Unsupported SQL cell type");
            }
        }
        if (!append("\r\n")) return fail("SQL export exceeds size limit");
    }
    if (cancel && cancel->load()) return fail("SQL export cancelled");
    return output;
}
bool saveSqlResultCsv(workspace::Project& project, std::string_view databasePath, std::string_view destination,
                      const SqlCsvExport& csv, std::string& error, const std::string* expected,
                      const std::atomic_bool* cancel) {
    error.clear();
    if (cancel && cancel->load()) { error = "SQL export cancelled"; return false; }
    if (!csv.error.empty() || csv.text.empty() || csv.text.size() > maxOutput || databasePath.empty() || destination.empty() ||
        databasePath.find('\0') != std::string_view::npos || destination.find('\0') != std::string_view::npos) {
        error = "Invalid SQL export or path"; return false;
    }
    std::filesystem::path input, output;
    try {
        input = std::filesystem::path(std::u8string(databasePath.begin(), databasePath.end()));
        output = std::filesystem::path(std::u8string(destination.begin(), destination.end()));
    } catch (const std::exception&) { error = "Invalid UTF-8 project path"; return false; }
    auto extension = output.extension().generic_u8string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](char8_t c) { return c >= u8'A' && c <= u8'Z' ? static_cast<char8_t>(c + (u8'a' - u8'A')) : c; });
    if (input.is_absolute() || output.is_absolute() || input.has_root_name() || output.has_root_name() ||
        input.has_root_directory() || output.has_root_directory() || extension != u8".csv") { error = "Choose a project-relative .csv destination"; return false; }
    if (!noSymlinkDestination(project.root(), output)) { error = "CSV destination cannot traverse symbolic links or reparse points"; return false; }
    const auto source = project.resolve(databasePath, error); if (source.empty()) return false;
    const auto target = project.resolve(destination, error); if (target.empty()) return false;
    std::error_code ec;
    if (!std::filesystem::is_regular_file(source, ec) || ec) { error = "Database file is unavailable"; return false; }
    const bool targetExists = std::filesystem::exists(target, ec);
    if (ec) { error = "Cannot inspect CSV destination"; return false; }
    if (source == target || (targetExists && std::filesystem::equivalent(source, target, ec))) {
        error = "CSV destination must differ from the database"; return false;
    }
    if (ec) { error = "Cannot compare CSV destination with database"; return false; }
    return project.writeFile(destination, csv.text, error, expected, !expected, cancel);
}
}
