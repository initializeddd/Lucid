#include "modules/DependencyGoZip.hpp"
#include "text/Utf8.hpp"
#include <picosha2.h>
#include <stb_image.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <set>
#include <vector>

namespace lucid::modules {
namespace {
constexpr std::size_t kZipLimit = 64U * 1024U * 1024U, kFileLimit = 16U * 1024U * 1024U, kExpandedLimit = 128U * 1024U * 1024U;
bool range(std::size_t offset, std::size_t count, std::size_t size) { return offset <= size && count <= size - offset; }
std::uint16_t le16(std::string_view s, std::size_t p) { return static_cast<unsigned char>(s[p]) | static_cast<std::uint16_t>(static_cast<unsigned char>(s[p + 1])) << 8; }
std::uint32_t le32(std::string_view s, std::size_t p) { return le16(s, p) | static_cast<std::uint32_t>(le16(s, p + 2)) << 16; }
std::uint32_t crc32(std::string_view data) {
    static const auto table = [] { std::array<std::uint32_t, 256> out{}; for (std::uint32_t i = 0; i < 256; ++i) { auto c = i; for (int j = 0; j < 8; ++j) c = (c >> 1) ^ (0xedb88320U & (0U - (c & 1U))); out[i] = c; } return out; }();
    std::uint32_t crc = 0xffffffffU; for (unsigned char c : data) crc = table[(crc ^ c) & 255U] ^ (crc >> 8); return crc ^ 0xffffffffU;
}
bool validName(std::string_view name, std::string_view prefix) {
    if (!name.starts_with(prefix) || name.size() <= prefix.size() || name.size() > 4096 || name.back() == '/') return false;
    std::size_t segment = prefix.size();
    for (std::size_t i = prefix.size(); i < name.size();) {
        const auto start = i; const auto cp = text::nextUtf8(name, i);
        if (cp < 32 || cp == 127 || cp == '\\' || (cp == U'\ufffd' && i - start == 1 && static_cast<unsigned char>(name[start]) >= 0x80)) return false;
        if (cp == '/') { const auto part = name.substr(segment, start - segment); if (part.empty() || part == "." || part == "..") return false; segment = i; }
    }
    const auto part = name.substr(segment); return !part.empty() && part != "." && part != "..";
}
bool validIdentity(std::string_view module, std::string_view version) {
    if (module.empty() || module.size() > 512 || module.front() == '/' || module.back() == '/' || version.size() < 2 || version.size() > 256 || version.front() != 'v') return false;
    std::size_t segment = 0;
    for (std::size_t i = 0; i <= module.size(); ++i) {
        if (i == module.size() || module[i] == '/') { const auto part = module.substr(segment, i - segment); if (part.empty() || part.front() == '.') return false; segment = i + 1; }
        else { const unsigned char c = module[i]; if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~' || c == '+')) return false; }
    }
    return std::all_of(version.begin() + 1, version.end(), [](unsigned char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '+'; });
}
std::string base64(const std::array<picosha2::byte_t, picosha2::k_digest_size>& data) {
    constexpr char chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"; std::string out; out.reserve(44);
    for (std::size_t i = 0; i < data.size(); i += 3) { auto n = std::min<std::size_t>(3, data.size() - i); unsigned x = static_cast<unsigned>(data[i]) << 16 | (n > 1 ? static_cast<unsigned>(data[i + 1]) << 8 : 0U) | (n > 2 ? data[i + 2] : 0U); out += chars[(x >> 18) & 63]; out += chars[(x >> 12) & 63]; out += n > 1 ? chars[(x >> 6) & 63] : '='; out += n > 2 ? chars[x & 63] : '='; }
    return out;
}
struct Entry { std::string name; std::uint32_t crc = 0, compressed = 0, size = 0, offset = 0; std::uint16_t flags = 0, method = 0; };
}
bool hashGoModuleZip(std::string_view bytes, std::string_view module, std::string_view version,
    std::string& hash, std::string& error, const std::atomic_bool* cancel) {
    hash.clear(); error.clear();
    const auto fail = [&](const char* message) { error = message; return false; };
    if (cancel && cancel->load()) return fail("Verification cancelled");
    if (bytes.size() < 22 || bytes.size() > kZipLimit || !validIdentity(module, version)) return fail("Invalid or oversized Go module ZIP");
    // Search only the ZIP-specified end-record window and require its comment to end at EOF.
    std::size_t eocd = std::string_view::npos, start = bytes.size() > 65557 ? bytes.size() - 65557 : 0;
    for (std::size_t p = bytes.size() - 22;; --p) { if (le32(bytes, p) == 0x06054b50U && range(p, 22, bytes.size()) && p + 22 + le16(bytes, p + 20) == bytes.size()) { eocd = p; break; } if (p == start) break; }
    if (eocd == std::string_view::npos || le16(bytes, eocd + 4) || le16(bytes, eocd + 6) || le16(bytes, eocd + 8) != le16(bytes, eocd + 10)) return fail("Unsupported ZIP end record");
    const auto count = le16(bytes, eocd + 10); const auto centralSize = le32(bytes, eocd + 12), centralOffset = le32(bytes, eocd + 16);
    if (!count || count > 10000 || count == 0xffff || centralSize == 0xffffffffU || centralOffset == 0xffffffffU || !range(centralOffset, centralSize, bytes.size()) || static_cast<std::size_t>(centralOffset) + centralSize != eocd) return fail("Unsupported ZIP directory");
    const std::string prefix = std::string(module) + "@" + std::string(version) + "/";
    std::vector<Entry> entries; entries.reserve(count); std::set<std::string> seen; std::size_t cursor = centralOffset, expanded = 0;
    for (std::size_t i = 0; i < count; ++i) {
        if (cancel && cancel->load()) return fail("Verification cancelled");
        if (!range(cursor, 46, eocd) || le32(bytes, cursor) != 0x02014b50U) return fail("Invalid ZIP directory entry");
        Entry entry; entry.flags = le16(bytes, cursor + 8); entry.method = le16(bytes, cursor + 10); entry.crc = le32(bytes, cursor + 16); entry.compressed = le32(bytes, cursor + 20); entry.size = le32(bytes, cursor + 24);
        const auto nameSize = le16(bytes, cursor + 28), extra = le16(bytes, cursor + 30), comment = le16(bytes, cursor + 32); entry.offset = le32(bytes, cursor + 42);
        const auto record = static_cast<std::size_t>(46) + nameSize + extra + comment;
        if (!range(cursor, record, eocd) || le16(bytes, cursor + 34) || (entry.flags & ~0x080eU) || (entry.method != 0 && entry.method != 8) || entry.size == 0xffffffffU || entry.compressed == 0xffffffffU || entry.offset == 0xffffffffU || entry.size > kFileLimit || entry.size > kExpandedLimit - expanded) return fail("Unsupported or oversized ZIP entry");
        entry.name = std::string(bytes.substr(cursor + 46, nameSize));
        if (!validName(entry.name, prefix) || !seen.insert(entry.name).second) return fail("Invalid or duplicate module ZIP path");
        expanded += entry.size; entries.push_back(std::move(entry)); cursor += record;
    }
    if (cursor != eocd) return fail("Invalid ZIP directory size");
    std::vector<std::pair<std::size_t, std::size_t>> regions; regions.reserve(entries.size());
    for (const auto& entry : entries) {
        const auto offset = static_cast<std::size_t>(entry.offset);
        if (!range(offset, 30, centralOffset) || le32(bytes, offset) != 0x04034b50U || le16(bytes, offset + 6) != entry.flags || le16(bytes, offset + 8) != entry.method) return fail("ZIP local header mismatch");
        const auto nameSize = le16(bytes, offset + 26), extra = le16(bytes, offset + 28); const auto header = static_cast<std::size_t>(30) + nameSize + extra;
        if (!range(offset, header, centralOffset) || bytes.substr(offset + 30, nameSize) != entry.name || !range(offset + header, entry.compressed, centralOffset)) return fail("Invalid ZIP local entry");
        const auto localCrc = le32(bytes, offset + 14), localCompressed = le32(bytes, offset + 18), localSize = le32(bytes, offset + 22);
        if (!(entry.flags & 8U) && (localCrc != entry.crc || localCompressed != entry.compressed || localSize != entry.size)) return fail("ZIP local size mismatch");
        auto end = offset + header + entry.compressed;
        if (entry.flags & 8U) {
            if (!range(end, 12, centralOffset)) return fail("Missing ZIP data descriptor");
            if (le32(bytes, end) == 0x08074b50U) { end += 4; if (!range(end, 12, centralOffset)) return fail("Invalid ZIP data descriptor"); }
            if (le32(bytes, end) != entry.crc || le32(bytes, end + 4) != entry.compressed || le32(bytes, end + 8) != entry.size) return fail("ZIP data descriptor mismatch");
            end += 12;
        }
        regions.emplace_back(offset, end);
    }
    std::sort(regions.begin(), regions.end());
    for (std::size_t i = 1; i < regions.size(); ++i) if (regions[i].first < regions[i - 1].second) return fail("Overlapping ZIP entries");
    std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) { return std::lexicographical_compare(a.name.begin(), a.name.end(), b.name.begin(), b.name.end(), [](unsigned char x, unsigned char y) { return x < y; }); });
    picosha2::hash256_one_by_one summary;
    for (const auto& entry : entries) {
        if (cancel && cancel->load()) return fail("Verification cancelled");
        const auto offset = static_cast<std::size_t>(entry.offset); const auto dataOffset = offset + 30U + le16(bytes, offset + 26) + le16(bytes, offset + 28);
        auto compressed = bytes.substr(dataOffset, entry.compressed); std::string expandedData;
        if (entry.method == 0) { if (entry.size != entry.compressed) return fail("Invalid stored ZIP entry"); expandedData.assign(compressed); }
        else {
            expandedData.resize(std::max<std::size_t>(1, entry.size));
            const auto output = stbi_zlib_decode_noheader_buffer(expandedData.data(), static_cast<int>(expandedData.size()), compressed.data(), static_cast<int>(compressed.size()));
            if (output < 0 || static_cast<std::size_t>(output) != entry.size) return fail("Invalid compressed ZIP entry");
            expandedData.resize(entry.size);
        }
        if (cancel && cancel->load()) return fail("Verification cancelled");
        if (crc32(expandedData) != entry.crc) return fail("ZIP entry checksum mismatch");
        const auto line = picosha2::hash256_hex_string(expandedData) + "  " + entry.name + "\n"; summary.process(line.begin(), line.end());
    }
    summary.finish(); std::array<picosha2::byte_t, picosha2::k_digest_size> digest{}; summary.get_hash_bytes(digest.begin(), digest.end()); hash = "h1:" + base64(digest); return true;
}
}
