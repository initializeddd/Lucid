#include "modules/LinkerMap.hpp"
#include <algorithm>
#include <charconv>
#include <limits>
#include <set>
namespace lucid::modules {
namespace {
constexpr std::size_t maxInput = 8 * 1024 * 1024, maxLine = 16384, maxRegions = 256, maxSections = 50000;
std::string_view trim(std::string_view s) { auto first = s.find_first_not_of(" \t\r"); if (first == s.npos) return {}; auto last = s.find_last_not_of(" \t\r"); return s.substr(first, last - first + 1); }
std::vector<std::string_view> words(std::string_view line) {
    std::vector<std::string_view> out;
    while (!(line = trim(line)).empty()) { auto end = line.find_first_of(" \t\r"); out.push_back(line.substr(0, end)); if (end == line.npos) break; line.remove_prefix(end); if (out.size() == 16) break; }
    return out;
}
bool number(std::string_view s, std::uint64_t& out) {
    if (s.size() < 3 || s[0] != '0' || (s[1] != 'x' && s[1] != 'X')) return false;
    s.remove_prefix(2); auto [end, ec] = std::from_chars(s.data(), s.data() + s.size(), out, 16); return ec == std::errc{} && end == s.data() + s.size();
}
bool range(std::uint64_t start, std::uint64_t size) { return size <= std::numeric_limits<std::uint64_t>::max() - start; }
}
bool parseGnuLinkerMap(std::string_view text, LinkerMap& result, std::string& error) {
    result = {}; error.clear();
    if (text.size() > maxInput) { error = "Linker map exceeds size limit."; return false; }
    LinkerMap parsed; bool memory = false, mapping = false, header = false, sawMemory = false, sawMap = false;
    std::set<std::string> regionNames; std::string pending;
    while (!text.empty()) {
        auto newline = text.find('\n'); auto line = text.substr(0, newline); if (newline == text.npos) text = {}; else text.remove_prefix(newline + 1);
        if (line.size() > maxLine) { error = "Linker map line exceeds size limit."; return false; }
        auto clean = trim(line);
        if (clean == "Memory Configuration") { memory = true; mapping = false; sawMemory = true; continue; }
        if (clean == "Linker script and memory map") { memory = false; mapping = true; sawMap = true; continue; }
        if (memory) {
            if (clean.empty()) continue;
            auto fields = words(clean);
            if (!header && fields.size() >= 3 && fields[0] == "Name" && fields[1] == "Origin" && fields[2] == "Length") { header = true; continue; }
            if (!header) continue;
            if (fields.size() < 3 || fields[0] == "*default*") continue;
            std::uint64_t origin = 0, length = 0;
            if (!number(fields[1], origin) || !number(fields[2], length) || !range(origin, length)) { error = "Invalid or overflowing memory region."; return false; }
            if (fields[0].size() > 256 || !regionNames.insert(std::string(fields[0])).second || parsed.regions.size() == maxRegions) { error = "Duplicate or excessive memory regions."; return false; }
            parsed.regions.push_back({std::string(fields[0]), fields.size() > 3 ? std::string(fields[3]) : std::string{}, origin, length, 0});
            continue;
        }
        if (!mapping || clean.empty()) continue;
        auto fields = words(clean);
        if (fields.empty()) continue;
        const bool unindented = !line.empty() && line[0] != ' ' && line[0] != '\t' && line[0] != '\r';
        std::uint64_t probeAddress = 0, probeSize = 0;
        const bool namedOutput = unindented && fields.size() >= 3 && fields[0] != "." &&
            !fields[0].starts_with("0x") && !fields[0].starts_with("0X") &&
            number(fields[1], probeAddress) && number(fields[2], probeSize);
        const bool dotOutput = unindented && fields[0].starts_with('.') && fields[0] != ".";
        const bool possibleWrapped = unindented && fields.size() == 1 && fields[0] != "." &&
            !fields[0].starts_with("0x") && !fields[0].starts_with("0X") &&
            fields[0].find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.$-") == std::string_view::npos;
        if (possibleWrapped) {
            if (!pending.empty()) parsed.incomplete = true;
            pending = std::string(fields[0]); if (pending.size() > 1024) { error = "Section name exceeds limit."; return false; } continue;
        }
        std::string section; std::size_t offset = 0;
        if (dotOutput || namedOutput) { if (!pending.empty()) parsed.incomplete = true; section = std::string(fields[0]); offset = 1; pending.clear(); }
        else if (!pending.empty() && (fields[0].starts_with("0x") || fields[0].starts_with("0X"))) { section = std::move(pending); pending.clear(); }
        else { if (!pending.empty()) parsed.incomplete = true; pending.clear(); continue; }
        if (fields.size() < offset + 2) { parsed.incomplete = true; continue; }
        std::uint64_t address = 0, size = 0;
        if (!number(fields[offset], address) || !number(fields[offset + 1], size) || !range(address, size)) { error = "Invalid or overflowing output section."; return false; }
        if (section.size() > 1024 || parsed.sections.size() == maxSections) { error = "Too many or oversized output sections."; return false; }
        LinkerSection entry; entry.name = std::move(section); entry.address = address; entry.size = size;
        for (std::size_t i = offset + 2; i < fields.size(); ++i) {
            if (fields[i] != "load") continue;
            if (entry.hasLoadAddress || i + 2 >= fields.size() || fields[i + 1] != "address" || !number(fields[i + 2], entry.loadAddress) || !range(entry.loadAddress, size)) {
                error = "Invalid or overflowing output section load address."; return false;
            }
            entry.hasLoadAddress = true; i += 2;
        }
        parsed.sections.push_back(std::move(entry));
    }
    if (!sawMemory || !header || !sawMap || parsed.regions.empty()) { error = "Missing supported GNU ld memory configuration or map."; return false; }
    using Ranges = std::vector<std::vector<std::pair<std::uint64_t, std::uint64_t>>>;
    Ranges ranges(parsed.regions.size()), loadRanges(parsed.regions.size());
    auto findRegion = [&](std::uint64_t address, std::uint64_t size) {
        int match = -1;
        for (std::size_t i = 0; i < parsed.regions.size(); ++i) {
            const auto& r = parsed.regions[i];
            if (address >= r.origin && address + size <= r.origin + r.length) {
                if (match != -1) return -2;
                match = static_cast<int>(i);
            }
        }
        return match;
    };
    for (auto& section : parsed.sections) {
        if (!section.size) continue;
        int match = findRegion(section.address, section.size);
        if (match < 0) parsed.incomplete = true;
        else { section.region = match; ranges[static_cast<std::size_t>(match)].emplace_back(section.address, section.address + section.size); }
        if (!section.hasLoadAddress) { parsed.loadIncomplete = true; continue; }
        match = findRegion(section.loadAddress, section.size);
        if (match < 0) { parsed.loadIncomplete = true; continue; }
        section.loadRegion = match;
        loadRanges[static_cast<std::size_t>(match)].emplace_back(section.loadAddress, section.loadAddress + section.size);
    }
    auto countUnion = [](auto& group) {
        std::sort(group.begin(), group.end()); std::uint64_t start = 0, end = 0, total = 0; bool active = false;
        for (auto [a, b] : group) { if (!active) { start = a; end = b; active = true; } else if (a <= end) end = std::max(end, b); else { total += end - start; start = a; end = b; } }
        if (active) total += end - start;
        return total;
    };
    for (std::size_t i = 0; i < ranges.size(); ++i) {
        parsed.regions[i].used = countUnion(ranges[i]);
        parsed.regions[i].loadUsed = countUnion(loadRanges[i]);
    }
    if (parsed.sections.empty() || !pending.empty()) { parsed.incomplete = true; parsed.loadIncomplete = true; }
    result = std::move(parsed); return true;
}
}
