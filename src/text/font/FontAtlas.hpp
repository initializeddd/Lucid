#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace lucid::text {

void setExecutablePath(std::string_view argv0);
[[nodiscard]] std::filesystem::path assetPath(std::string_view relative);

struct Glyph {
    float x0 = 0.0F;
    float y0 = 0.0F;
    float x1 = 0.0F;
    float y1 = 0.0F;
    float u0 = 0.0F;
    float v0 = 0.0F;
    float u1 = 0.0F;
    float v1 = 0.0F;
    float advance = 0.0F;
    bool hasPixels = false;
};

class FontAtlas {
public:
    static constexpr int kFirstCodepoint = 32;
    static constexpr int kLastCodepoint = 126;
    static constexpr int kGlyphCount = kLastCodepoint - kFirstCodepoint + 1;
    static constexpr char32_t kCommandCodepoint = 0x2318;
    static constexpr char32_t kShiftCodepoint = 0x21E7;
    static constexpr char32_t kControlCodepoint = 0x2303;

    explicit FontAtlas(int pixelHeight);

    [[nodiscard]] const Glyph& glyph(char32_t codepoint) const noexcept;
    [[nodiscard]] int width() const noexcept { return m_width; }
    [[nodiscard]] int height() const noexcept { return m_height; }
    [[nodiscard]] int pixelHeight() const noexcept { return m_pixelHeight; }
    [[nodiscard]] float ascent() const noexcept { return m_ascent; }
    [[nodiscard]] float descent() const noexcept { return m_descent; }
    [[nodiscard]] const std::vector<std::uint8_t>& pixels() const noexcept { return m_pixels; }
    [[nodiscard]] const std::string& fontPath() const noexcept { return m_fontPath; }

private:
    int m_width = 0;
    int m_height = 0;
    int m_pixelHeight = 0;
    float m_ascent = 0.0F;
    float m_descent = 0.0F;
    std::array<Glyph, kGlyphCount> m_glyphs{};
    Glyph m_commandGlyph{};
    Glyph m_shiftGlyph{};
    Glyph m_controlGlyph{};
    std::vector<std::uint8_t> m_pixels;
    std::string m_fontPath;
};

} // namespace lucid::text
