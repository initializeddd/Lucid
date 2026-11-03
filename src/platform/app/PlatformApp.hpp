#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace lucid::platform {

struct MenuActions {
    std::function<void()> openProject;
    std::function<void()> openFile;
    std::function<void()> save;
    std::function<void()> closeFile;
    std::function<void()> deleteFile;
    std::function<void()> deleteProject;
    std::function<void()> settings;
    std::function<void()> runCompile;
    std::function<void()> runFunction;
    std::function<void()> closeTerminal;
    std::function<void()> undo;
    std::function<void()> redo;
    std::function<void()> cut;
    std::function<void()> copy;
    std::function<void()> paste;
    std::function<void()> selectAll;
    std::function<void()> complete;
    std::function<void()> askAi;
    std::function<void()> find;
    std::function<void()> replace;
    std::function<void()> switchFile;
    std::function<void()> tasks;
    std::function<void()> gitReview;
    std::function<void()> aiCleanup;
    std::function<void()> modules;
    std::function<void()> recovery;
    std::function<void()> zoomIn;
    std::function<void()> zoomOut;
    std::function<void()> zoomReset;
    std::function<void()> contrast;
    std::function<void()> debugger;
    std::function<void()> breakpoint;
};

[[nodiscard]] bool openBinaryFile(std::string_view path);

[[nodiscard]] std::vector<unsigned char> makeThemedIcon(float red, float green, float blue, std::string_view theme);
void setThemedApplicationIcon(void* nativeWindow, float red, float green, float blue, std::string_view theme);
void prepareOpenFiles();
void setOpenFileHandler(std::function<void(std::string)> handler);
void installApplicationMenu(const MenuActions& actions);
void configureApplicationWindow(void* nativeWindow, std::string_view title);
void updateAccessibility(void* nativeWindow, std::string_view label, std::string_view value,
                         std::size_t selectionStart, std::size_t selectionLength);
[[nodiscard]] bool confirmDestructiveAction(std::string_view title,
                                            std::string_view message,
                                            std::string_view confirmLabel);

} // namespace lucid::platform
