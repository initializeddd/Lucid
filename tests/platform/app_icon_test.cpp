#include "platform/app/PlatformApp.hpp"
#include <cstddef>
int main() {
    const auto red = lucid::platform::makeThemedIcon(1, 0, 0, "classic");
    const auto blue = lucid::platform::makeThemedIcon(0, 0, 1, "classic");
    const auto filled = lucid::platform::makeThemedIcon(1, 0, 0, "accent");
    const auto outline = lucid::platform::makeThemedIcon(1, 0, 0, "outline");
    const std::size_t mark = (40 * 128 + 35) * 4;
    const std::size_t background = (64 * 128 + 15) * 4;
    if (red.size() != 128 * 128 * 4 || red[mark] != 255 || red[mark + 2] != 0) return 1;
    if (blue[mark] != 0 || blue[mark + 2] != 255) return 2;
    if (filled[background] != 255 || red[background] == filled[background]) return 3;
    if (outline[background + 3] != 0 || red[background + 3] != 255) return 4;
    return 0;
}
