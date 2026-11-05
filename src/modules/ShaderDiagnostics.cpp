#include "modules/ShaderDiagnostics.hpp"
#include <algorithm>
#include <charconv>
namespace lucid::modules {
namespace {
std::string_view trim(std::string_view s) { while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r')) s.remove_prefix(1); while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) s.remove_suffix(1); return s; }
bool number(std::string_view s, std::size_t& n) { s = trim(s); if (s.empty()) return false; auto [p, ec] = std::from_chars(s.data(), s.data() + s.size(), n); return ec == std::errc{} && p == s.data() + s.size() && n > 0; }
bool parse(std::string_view text, ShaderDiagnostic& d) {
    text = trim(text); std::string_view severity, rest;
    // glslang: ERROR: filename:line: message (filename may contain a drive colon).
    if (text.starts_with("ERROR:") || text.starts_with("WARNING:")) {
        const bool error = text.starts_with("ERROR:"); severity = error ? "error" : "warning"; rest = trim(text.substr(error ? 6 : 8));
        for (std::size_t sep = rest.find(':'); sep != std::string_view::npos; sep = rest.find(':', sep + 1)) {
            auto end = rest.find(':', sep + 1); if (end == std::string_view::npos) break;
            std::size_t line = 0; if (!number(rest.substr(sep + 1, end - sep - 1), line)) continue;
            auto path = trim(rest.substr(0, sep)), msg = trim(rest.substr(end + 1)); if (path.empty() || msg.empty()) return false;
            d = {std::string(path), std::string(severity), std::string(msg), line, 0}; return true;
        }
        return false;
    }
    // clang/Metal: filename:line[:column]: error|warning|note: message.
    constexpr std::string_view levels[] = {"error", "warning", "note"};
    for (auto level : levels) {
        auto marker = std::string(": ") + std::string(level) + ":";
        auto at = text.find(marker); if (at == std::string_view::npos) continue;
        auto location = text.substr(0, at), msg = trim(text.substr(at + marker.size()));
        auto last = location.rfind(':'); if (last == std::string_view::npos || msg.empty()) continue;
        std::size_t trailing = 0; if (!number(location.substr(last + 1), trailing)) continue;
        auto prior = location.rfind(':', last - (last > 0 ? 1 : 0)); std::size_t line = trailing, column = 0; auto path = location.substr(0, last);
        if (prior != std::string_view::npos) {
            auto possibleLine = location.substr(prior + 1, last - prior - 1); std::size_t candidate = 0;
            if (number(possibleLine, candidate)) { line = candidate; column = trailing; path = location.substr(0, prior); }
            else if (!possibleLine.empty() && std::all_of(possibleLine.begin(), possibleLine.end(), [](char c) { return c >= '0' && c <= '9'; })) return false;
        }
        path = trim(path); if (path.empty()) continue;
        d = {std::string(path), std::string(level), std::string(msg), line, column}; return true;
    }
    return false;
}
}
ShaderDiagnostics parseShaderDiagnostics(std::string_view output, std::size_t maxBytes, std::size_t maxLines, std::size_t maxDiagnostics) {
    ShaderDiagnostics result; std::size_t pos = 0;
    while (pos < output.size()) {
        if (pos >= maxBytes || result.scannedLines >= maxLines) { result.incomplete = true; break; }
        const auto window = output.substr(pos, std::min(maxBytes - pos, output.size() - pos)); auto nl = window.find('\n');
        if (nl == std::string_view::npos && window.size() < output.size() - pos) { result.incomplete = true; break; }
        const auto len = nl == std::string_view::npos ? window.size() : nl; auto line = window.substr(0, len); pos += len + (nl == std::string_view::npos ? 0 : 1); ++result.scannedLines;
        if (trim(line).empty()) continue;
        ShaderDiagnostic diagnostic; if (!parse(line, diagnostic)) { ++result.unparsedLines; continue; }
        if (result.diagnostics.size() >= maxDiagnostics) result.incomplete = true; else result.diagnostics.push_back(std::move(diagnostic));
    }
    return result;
}
}
