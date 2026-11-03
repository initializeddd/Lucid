#include "platform/app/PlatformApp.hpp"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cstdint>
#include <vector>

namespace lucid::platform {
#if defined(__APPLE__)
void setNativeAppIcon(const unsigned char* rgba, int width, int height);
#endif
std::vector<unsigned char> makeThemedIcon(float red, float green, float blue,
                                          std::string_view theme) {
    constexpr int size = 128;
    std::vector<unsigned char> pixels(size * size * 4, 0);
    const auto channel = [](float c) { return static_cast<unsigned char>(std::clamp(c, 0.0F, 1.0F) * 255.0F + 0.5F); };
    const unsigned char accent[3] = {channel(red), channel(green), channel(blue)};
    for (int y = 0; y < size; ++y) for (int x = 0; x < size; ++x) {
        const int dx = std::max({20 - x, 0, x - 107});
        const int dy = std::max({20 - y, 0, y - 107});
        if (dx * dx + dy * dy > 400) continue;
        auto* px = &pixels[static_cast<std::size_t>((y * size + x) * 4)];
        const bool edge = x < 8 || y < 8 || x >= size - 8 || y >= size - 8;
        const bool bar = x >= 30 && x < 45 && y >= 27 && y < 101;
        const bool letter = (x >= 55 && x < 70 && y >= 27 && y < 101) ||
                            (x >= 55 && x < 101 && y >= 86 && y < 101);
        const bool filled = theme == "accent";
        const bool outline = theme == "outline";
        for (int c = 0; c < 3; ++c) {
            px[c] = filled ? accent[c] : static_cast<unsigned char>(22 + c * 3);
            if ((bar && !filled) || (outline && edge)) px[c] = accent[c];
            if (letter || (bar && filled)) px[c] = 245;
        }
        px[3] = outline && !edge && !bar && !letter ? 0 : 255;
    }
    return pixels;
}
void setThemedApplicationIcon(void* nativeWindow, float red, float green, float blue,
                              std::string_view theme) {
    constexpr int size = 128;
    auto pixels = makeThemedIcon(red, green, blue, theme);
#if defined(__APPLE__)
    (void)nativeWindow;
    setNativeAppIcon(pixels.data(), size, size);
#else
    if (nativeWindow) {
        GLFWimage image{size, size, pixels.data()};
        glfwSetWindowIcon(static_cast<GLFWwindow*>(nativeWindow), 1, &image);
    }
#endif
}
}
