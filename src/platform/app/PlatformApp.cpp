#include "platform/app/PlatformApp.hpp"

#if defined(_WIN32)
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <dwmapi.h>
#include <windows.h>
#include <shellapi.h>

#include <string>

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef DWMWA_BORDER_COLOR
#define DWMWA_BORDER_COLOR 34
#endif
#ifndef DWMWA_CAPTION_COLOR
#define DWMWA_CAPTION_COLOR 35
#endif
#ifndef DWMWA_TEXT_COLOR
#define DWMWA_TEXT_COLOR 36
#endif
#endif

#if !defined(_WIN32) && !defined(__APPLE__)
#include <sys/wait.h>
#include <unistd.h>
#include <string>
#endif

namespace lucid::platform {

#if defined(_WIN32)
bool openBinaryFile(std::string_view path) {
    if (!confirmDestructiveAction("Open binary file?", "Open this binary file with the default application?", "Open")) return false;
    const std::string file(path);
    return reinterpret_cast<intptr_t>(ShellExecuteA(nullptr, "open", file.c_str(), nullptr, nullptr, SW_SHOWNORMAL)) > 32;
}
#elif !defined(__APPLE__)
bool openBinaryFile(std::string_view path) {
    const pid_t prompt = fork();
    if (prompt == 0) {
        execlp("zenity", "zenity", "--question", "--title=Open binary file?", "--text=Open this binary file with the default application?", "--ok-label=Open", static_cast<char*>(nullptr));
        execlp("kdialog", "kdialog", "--yesno", "Open this binary file with the default application?", "--title", "Open binary file?", static_cast<char*>(nullptr));
        _exit(127);
    }
    int status = 0;
    if (prompt < 0 || waitpid(prompt, &status, 0) < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) return false;
    const std::string file(path);
    const pid_t child = fork();
    if (child == 0) {
        execlp("xdg-open", "xdg-open", file.c_str(), static_cast<char*>(nullptr));
        _exit(127);
    }
    return child > 0 && waitpid(child, &status, 0) > 0 && WIFEXITED(status) && WEXITSTATUS(status) == 0;
}
#endif


#if !defined(__APPLE__)
void installApplicationMenu(const MenuActions& /*actions*/) {}
#endif

#if defined(_WIN32)
void configureApplicationWindow(void* nativeWindow, std::string_view title) {
    if (nativeWindow == nullptr) {
        return;
    }

    auto* window = static_cast<GLFWwindow*>(nativeWindow);
    const std::string windowTitle(title);
    glfwSetWindowTitle(window, windowTitle.c_str());

    HWND handle = glfwGetWin32Window(window);
    if (handle == nullptr) {
        return;
    }

    const BOOL dark = TRUE;
    const COLORREF caption = RGB(20, 22, 30);
    const COLORREF text = RGB(232, 234, 242);
    const COLORREF border = RGB(42, 45, 58);
    DwmSetWindowAttribute(handle, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
    DwmSetWindowAttribute(handle, DWMWA_CAPTION_COLOR, &caption, sizeof(caption));
    DwmSetWindowAttribute(handle, DWMWA_TEXT_COLOR, &text, sizeof(text));
    DwmSetWindowAttribute(handle, DWMWA_BORDER_COLOR, &border, sizeof(border));
}
#elif !defined(__APPLE__)
void configureApplicationWindow(void* /*nativeWindow*/, std::string_view /*title*/) {}
#endif

#if defined(_WIN32)
bool confirmDestructiveAction(std::string_view title, std::string_view message,
                              std::string_view /*confirmLabel*/) {
    const std::string titleText(title);
    const std::string messageText(message);
    return MessageBoxA(nullptr, messageText.c_str(), titleText.c_str(),
                       MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) == IDYES;
}
#else
bool confirmDestructiveAction(std::string_view /*title*/,
                              std::string_view /*message*/,
                              std::string_view /*confirmLabel*/) {
    return false;
}
#endif

} // namespace lucid::platform
