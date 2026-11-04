#include <cstdio>
#include <cstring>
#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#endif
int main(int argc, char** argv) {
    if (argc == 2 && std::strcmp(argv[1], "--diagnostic-failure") == 0) {
        std::fputs("fixture diagnostic failure\n", stderr);
        std::fflush(stderr);
        return 42;
    }
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
