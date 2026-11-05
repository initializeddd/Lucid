#include "modules/Coverage.hpp"
#include <algorithm>
#include <charconv>
#include <exception>
#include <filesystem>
#include <map>
#include <system_error>
#include <tuple>

namespace lucid::modules {
CoverageReport parseLcov(const workspace::Project& project, std::string_view report, const std::atomic_bool* cancel, std::size_t limit) {
    CoverageReport result; std::map<std::pair<std::string, std::size_t>, bool> lines;
    // -1 means no execution count was recorded; 0 and 1 are known outcomes.
    std::map<std::tuple<std::string,std::size_t,std::string,std::string>, int> branches;
    const auto branchLimit = limit > 6250 ? std::size_t{100000} : limit * 16;
    if (report.size() > workspace::Project::maxFileSize) { result.incomplete = true; return result; }
    std::error_code ec; const auto root = std::filesystem::weakly_canonical(project.root(), ec);
    if (ec || root.empty()) { result.incomplete = true; return result; }
    std::string file; bool inSection = false, validFile = false, sawSection = false;
    for (std::size_t pos = 0; pos < report.size();) {
        if (cancel && cancel->load()) { result.incomplete = true; break; }
        const auto end = report.find('\n', pos); auto line = report.substr(pos, end == std::string_view::npos ? report.size() - pos : end - pos);
        pos = end == std::string_view::npos ? report.size() : end + 1;
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        if (line.starts_with("SF:") || line.starts_with("KF:")) {
            if (inSection) result.incomplete = true;
            inSection = true; sawSection = true; validFile = false; file.clear();
            const auto name = line.substr(3);
            if (name.empty() || name.find('\0') != std::string_view::npos) { ++result.skippedFiles; result.incomplete = true; continue; }
            std::filesystem::path path;
            try { path = std::filesystem::path(std::u8string(name.begin(), name.end())); }
            catch (const std::exception&) { ++result.skippedFiles; result.incomplete = true; continue; }
            if (path.is_absolute()) {
                const auto canonical = std::filesystem::weakly_canonical(path, ec);
                if (ec) { ec.clear(); ++result.skippedFiles; result.incomplete = true; continue; }
                path = canonical.lexically_relative(root);
            }
            std::string error, relative;
            try { const auto utf8 = path.lexically_normal().generic_u8string(); relative.assign(utf8.begin(), utf8.end()); }
            catch (const std::exception&) { ++result.skippedFiles; result.incomplete = true; continue; }
            const auto resolved = project.resolve(relative, error);
            if (resolved.empty() || !project.exists(relative) || project.isDirectory(relative)) { ++result.skippedFiles; result.incomplete = true; continue; }
            file = relative; validFile = true;
        } else if (line == "end_of_record") {
            if (!inSection) result.incomplete = true;
            inSection = false; validFile = false;
        } else if (line.starts_with("BRDA:")) {
            if (!inSection) { result.incomplete = true; continue; }
            if (!validFile) continue;
            auto data = line.substr(5); const auto a = data.find(','), b = a == std::string_view::npos ? a : data.find(',', a + 1), c = b == std::string_view::npos ? b : data.find(',', b + 1);
            if (a == std::string_view::npos || b == std::string_view::npos || c == std::string_view::npos || data.find(',', c + 1) != std::string_view::npos) { result.incomplete = true; continue; }
            const auto numberText = data.substr(0,a), block = data.substr(a+1,b-a-1), branch = data.substr(b+1,c-b-1), taken = data.substr(c+1);
            std::size_t number = 0; unsigned long long hits = 0; const auto parsed = std::from_chars(numberText.data(),numberText.data()+numberText.size(),number);
            const auto validId = [](std::string_view id) { return !id.empty() && id.size() <= 64 && std::all_of(id.begin(),id.end(),[](unsigned char ch) { return ch >= 33 && ch < 127; }); };
            const auto hitResult = std::from_chars(taken.data(),taken.data()+taken.size(),hits);
            if (parsed.ec != std::errc{} || parsed.ptr != numberText.data()+numberText.size() || !number || !validId(block) || !validId(branch) || (taken != "-" && (hitResult.ec != std::errc{} || hitResult.ptr != taken.data()+taken.size()))) { result.incomplete = true; continue; }
            const auto key = std::make_tuple(file,number-1,std::string(block),std::string(branch)); const int outcome = taken == "-" ? -1 : hits ? 1 : 0;
            auto found = branches.find(key);
            if (found != branches.end()) found->second = std::max(found->second, outcome);
            else if (branches.size() >= branchLimit) { result.incomplete = true; break; }
            else branches.emplace(key,outcome);
        } else if (line.starts_with("DA:")) {
            if (!inSection) { result.incomplete = true; continue; }
            if (!validFile) continue;
            auto data = line.substr(3); const auto comma = data.find(',');
            if (comma == std::string_view::npos) { result.incomplete = true; continue; }
            const auto number = data.substr(0, comma); data.remove_prefix(comma + 1); const auto next = data.find(','); const auto countText = data.substr(0, next);
            std::size_t numberValue = 0; unsigned long long hits = 0;
            const auto a = std::from_chars(number.data(), number.data() + number.size(), numberValue);
            const auto b = std::from_chars(countText.data(), countText.data() + countText.size(), hits);
            if (a.ec != std::errc{} || a.ptr != number.data() + number.size() || numberValue == 0 || b.ec != std::errc{} || b.ptr != countText.data() + countText.size()) { result.incomplete = true; continue; }
            const auto key = std::make_pair(file, numberValue - 1);
            auto found = lines.find(key);
            if (found == lines.end()) { if (lines.size() >= limit) { result.incomplete = true; break; } lines.emplace(key, hits != 0); }
            else found->second = found->second || hits != 0;
        }
    }
    if (inSection || !sawSection) result.incomplete = true;
    for (const auto& [key, covered] : lines) result.lines.push_back({key.first, key.second, covered});
    std::map<std::pair<std::string,std::size_t>, std::size_t> indices;
    for (std::size_t i=0; i<result.lines.size(); ++i) indices.emplace(std::make_pair(result.lines[i].path,result.lines[i].line),i);
    for (const auto& [key, outcome] : branches) {
        const auto found = indices.find({std::get<0>(key),std::get<1>(key)});
        if (found == indices.end()) { result.incomplete = true; continue; }
        auto& line = result.lines[found->second];
        if (outcome > 0) ++line.branchesCovered; else if (outcome == 0) ++line.branchesUncovered; else ++line.branchesUnknown;
    }
    return result;
}
}
