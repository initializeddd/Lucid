#include "text/font/FontAtlas.hpp"

#include "logging/Log.hpp"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

namespace lucid::text {
namespace {
std::filesystem::path g_launchPath;


[[nodiscard]] std::vector<std::uint8_t> readFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        return {};
    }

    const std::streampos end = file.tellg();
    if (end <= 0) {
        return {};
    }

    std::vector<std::uint8_t> data(static_cast<std::size_t>(end));
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));
    if (!file) {
        return {};
    }
    return data;
}

[[nodiscard]] std::filesystem::path executablePath() {
#if defined(_WIN32)
    std::wstring buffer(32768, L'\0');
    const DWORD length =
        GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0 || static_cast<std::size_t>(length) >= buffer.size()) {
        return {};
    }
    buffer.resize(length);
    return std::filesystem::path(buffer);
#elif defined(__APPLE__)
    std::uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::string buffer(size, '\0');
    if (_NSGetExecutablePath(buffer.data(), &size) != 0) {
        return {};
    }
    if (!buffer.empty() && buffer.back() == '\0') {
        buffer.pop_back();
    }
    std::error_code error;
    const std::filesystem::path canonical = std::filesystem::canonical(buffer, error);
    return error ? std::filesystem::path(buffer) : canonical;
#else
    if (!g_launchPath.empty()) return g_launchPath;
    std::error_code error;
    const std::filesystem::path path = std::filesystem::read_symlink("/proc/self/exe", error);
    return error ? std::filesystem::path() : path;
#endif
}

[[nodiscard]] std::vector<std::filesystem::path> fontCandidates() {
    std::vector<std::filesystem::path> candidates;

    if (const char* overridePath = std::getenv("LUCID_FONT"); overridePath != nullptr) {
        candidates.emplace_back(overridePath);
    }

#if defined(__APPLE__)
    candidates.emplace_back("/System/Library/Fonts/Menlo.ttc");
    candidates.emplace_back("/System/Library/Fonts/HelveticaNeue.ttc");
    candidates.emplace_back("/System/Library/Fonts/Supplemental/Verdana.ttf");
    candidates.emplace_back("/System/Library/Fonts/SFNS.ttf");
    candidates.emplace_back("/System/Library/Fonts/Supplemental/Arial.ttf");
#elif defined(_WIN32)
    candidates.emplace_back("C:/Windows/Fonts/segoeui.ttf");
    candidates.emplace_back("C:/Windows/Fonts/consola.ttf");
    candidates.emplace_back("C:/Windows/Fonts/arial.ttf");
#else
    candidates.emplace_back("/usr/local/share/fonts/dejavu/DejaVuSansMono.ttf");
    candidates.emplace_back("/usr/local/share/fonts/TTF/DejaVuSansMono.ttf");
    candidates.emplace_back("/usr/X11R6/lib/X11/fonts/TTF/DejaVuSansMono.ttf");
    candidates.emplace_back("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");
    candidates.emplace_back("/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf");
    candidates.emplace_back("/usr/share/fonts/TTF/DejaVuSans.ttf");
    candidates.emplace_back("/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf");
#endif

    // Release builds ship a font so systems without installed fonts still work.
    const std::filesystem::path executable = executablePath();
    if (!executable.empty()) {
        const std::filesystem::path directory = executable.parent_path();
        candidates.emplace_back(directory / "assets" / "fonts" /
                                "JetBrainsMono-Regular.ttf");
        candidates.emplace_back(directory / ".." / "Resources" / "assets" / "fonts" /
                                "JetBrainsMono-Regular.ttf");
    }
    candidates.emplace_back(std::filesystem::current_path() / "assets" / "fonts" /
                            "JetBrainsMono-Regular.ttf");

    return candidates;
}

} // namespace

std::filesystem::path applicationExecutablePath() { return executablePath(); }

std::filesystem::path assetPath(std::string_view relative) {
    const auto executable = executablePath();
    std::vector<std::filesystem::path> roots;
    if (!executable.empty()) {
        const auto directory = executable.parent_path();
        roots.push_back(directory / "assets");
        roots.push_back(directory / ".." / "Resources" / "assets");
    }
    roots.push_back(std::filesystem::current_path() / "assets");
    for (const auto& root : roots) {
        const auto candidate = root / std::filesystem::path(relative);
        std::error_code ec;
        if (std::filesystem::is_regular_file(candidate, ec) && !ec) return candidate;
    }
    return {};
}

void setExecutablePath(std::string_view argv0) {
    if (argv0.empty()) return;
    std::filesystem::path path{std::string(argv0)};
    std::error_code error;
    if (path.has_parent_path()) {
        g_launchPath = std::filesystem::canonical(path, error);
        return;
    }
    const char* search = std::getenv("PATH");
    if (!search) return;
    std::string_view paths(search);
    while (true) {
        const auto separator = paths.find(':');
        const auto directory = paths.substr(0, separator);
        const auto candidate = std::filesystem::path(directory.empty() ? "." : std::string(directory)) / path;
        error.clear();
        auto resolved = std::filesystem::canonical(candidate, error);
        if (!error && std::filesystem::is_regular_file(resolved, error) && !error) { g_launchPath = std::move(resolved); return; }
        if (separator == std::string_view::npos) break;
        paths.remove_prefix(separator + 1);
    }
}

FontAtlas::FontAtlas(int pixelHeight) : m_pixelHeight(pixelHeight) {
    if (pixelHeight <= 0) {
        throw std::runtime_error("Font pixel height must be positive");
    }

    constexpr int kFirstAtlasSize = 1024;
    constexpr int kSecondAtlasSize = 2048;

    std::string attempted;
    for (const std::filesystem::path& candidate : fontCandidates()) {
        std::error_code error;
        if (!std::filesystem::exists(candidate, error) || error) {
            continue;
        }

        attempted += attempted.empty() ? candidate.string() : ", " + candidate.string();
        std::vector<std::uint8_t> fontData = readFile(candidate);
        if (fontData.empty()) {
            continue;
        }

        stbtt_fontinfo fontInfo{};
        const int offset = stbtt_GetFontOffsetForIndex(fontData.data(), 0);
        if (offset < 0 || stbtt_InitFont(&fontInfo, fontData.data(), offset) == 0) {
            continue;
        }

        for (const int atlasSize : {kFirstAtlasSize, kSecondAtlasSize}) {
            std::vector<std::uint8_t> bitmap(static_cast<std::size_t>(atlasSize) *
                                             static_cast<std::size_t>(atlasSize),
                                             0);
            std::array<stbtt_packedchar, kGlyphCount> packed{};

            stbtt_pack_context packContext{};
            if (stbtt_PackBegin(&packContext, bitmap.data(), atlasSize, atlasSize, 0, 16,
                                nullptr) == 0) {
                continue;
            }

            stbtt_PackSetOversampling(&packContext, 1, 1);
            const int packedResult = stbtt_PackFontRange(
                &packContext, fontData.data(), 0, static_cast<float>(pixelHeight),
                kFirstCodepoint, kGlyphCount, packed.data());

            stbtt_packedchar packedCommand{};
            const int commandResult = stbtt_PackFontRange(
                &packContext, fontData.data(), 0, static_cast<float>(pixelHeight),
                static_cast<int>(kCommandCodepoint), 1, &packedCommand);

            stbtt_packedchar packedShift{};
            const int shiftResult = stbtt_PackFontRange(
                &packContext, fontData.data(), 0, static_cast<float>(pixelHeight),
                static_cast<int>(kShiftCodepoint), 1, &packedShift);

            stbtt_packedchar packedControl{};
            const int controlResult = stbtt_PackFontRange(
                &packContext, fontData.data(), 0, static_cast<float>(pixelHeight),
                static_cast<int>(kControlCodepoint), 1, &packedControl);
            std::array<stbtt_packedchar, kPunctuationCodepoints.size()> packedPunctuation{};
            bool punctuationPacked = true;
            for (std::size_t i = 0; i < packedPunctuation.size(); ++i) {
                punctuationPacked &= stbtt_PackFontRange(&packContext, fontData.data(), 0,
                    static_cast<float>(pixelHeight), static_cast<int>(kPunctuationCodepoints[i]),
                    1, &packedPunctuation[i]) != 0;
            }
            stbtt_PackEnd(&packContext);

            if (packedResult == 0 || !punctuationPacked) {
                continue;
            }

            const float scale = stbtt_ScaleForPixelHeight(&fontInfo, static_cast<float>(pixelHeight));
            int ascent = 0;
            int descent = 0;
            int lineGap = 0;
            stbtt_GetFontVMetrics(&fontInfo, &ascent, &descent, &lineGap);
            m_ascent = static_cast<float>(ascent) * scale;
            m_descent = static_cast<float>(descent) * scale;

            for (int index = 0; index < kGlyphCount; ++index) {
                stbtt_aligned_quad quad{};
                float xpos = 0.0F;
                float ypos = 0.0F;
                stbtt_GetPackedQuad(packed.data(), atlasSize, atlasSize, index, &xpos, &ypos, &quad,
                                    1);

                Glyph& glyph = m_glyphs[static_cast<std::size_t>(index)];
                glyph.x0 = quad.x0;
                glyph.y0 = quad.y0;
                glyph.x1 = quad.x1;
                glyph.y1 = quad.y1;
                glyph.u0 = quad.s0;
                glyph.v0 = quad.t0;
                glyph.u1 = quad.s1;
                glyph.v1 = quad.t1;
                glyph.advance = xpos;
                glyph.hasPixels = quad.x1 > quad.x0 + 0.01F && quad.y1 > quad.y0 + 0.01F;
            }

            const auto fillExtraGlyph = [&](stbtt_packedchar& packed,
                                            Glyph& glyph) {
                stbtt_aligned_quad quad{};
                float xpos = 0.0F;
                float ypos = 0.0F;
                stbtt_GetPackedQuad(&packed, atlasSize, atlasSize, 0, &xpos,
                                    &ypos, &quad, 1);
                glyph.x0 = quad.x0;
                glyph.y0 = quad.y0;
                glyph.x1 = quad.x1;
                glyph.y1 = quad.y1;
                glyph.u0 = quad.s0;
                glyph.v0 = quad.t0;
                glyph.u1 = quad.s1;
                glyph.v1 = quad.t1;
                glyph.advance = xpos;
                glyph.hasPixels = quad.x1 > quad.x0 + 0.01F &&
                                  quad.y1 > quad.y0 + 0.01F;
            };

            m_commandGlyph = Glyph{};
            if (commandResult != 0) {
                fillExtraGlyph(packedCommand, m_commandGlyph);
            }
            m_shiftGlyph = Glyph{};
            if (shiftResult != 0) {
                fillExtraGlyph(packedShift, m_shiftGlyph);
            }
            m_controlGlyph = Glyph{};
            if (controlResult != 0) {
                fillExtraGlyph(packedControl, m_controlGlyph);
            }
            for (std::size_t i = 0; i < packedPunctuation.size(); ++i)
                fillExtraGlyph(packedPunctuation[i], m_punctuationGlyphs[i]);

            m_width = atlasSize;
            m_height = atlasSize;
            m_pixels = std::move(bitmap);
            m_fontPath = candidate.string();
            log::info("Font: " + m_fontPath + " (" + std::to_string(pixelHeight) + "px, " +
                      std::to_string(m_width) + "x" + std::to_string(m_height) + " atlas).");
            return;
        }
    }

    throw std::runtime_error("Could not load a UI font. Tried: " +
                             (attempted.empty() ? std::string("no installed fonts") : attempted));
}

const Glyph& FontAtlas::glyph(char32_t codepoint) const noexcept {
    for (std::size_t i = 0; i < kPunctuationCodepoints.size(); ++i)
        if (codepoint == kPunctuationCodepoints[i]) return m_punctuationGlyphs[i];
    if (codepoint == kCommandCodepoint) {
        return m_commandGlyph;
    }
    if (codepoint == kShiftCodepoint) {
        return m_shiftGlyph;
    }
    if (codepoint == kControlCodepoint) {
        return m_controlGlyph;
    }
    if (codepoint < static_cast<char32_t>(kFirstCodepoint) ||
        codepoint > static_cast<char32_t>(kLastCodepoint)) {
        codepoint = static_cast<char32_t>('?');
    }
    return m_glyphs[static_cast<std::size_t>(codepoint - kFirstCodepoint)];
}

} // namespace lucid::text
