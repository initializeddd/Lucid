#include <cstdio>
#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#endif
int main() {
#if defined(_WIN32)
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    char buffer[4096];
    while (const auto count = std::fread(buffer, 1, sizeof(buffer), stdin)) {
        if (std::fwrite(buffer, 1, count, stdout) != count || std::fflush(stdout)) return 1;
    }
    return 0;
}
