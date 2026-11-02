#pragma once

#include <functional>
#include <string_view>

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
};

[[nodiscard]] bool openBinaryFile(std::string_view path);

void installApplicationMenu(const MenuActions& actions);
void configureApplicationWindow(void* nativeWindow, std::string_view title);
[[nodiscard]] bool confirmDestructiveAction(std::string_view title,
                                            std::string_view message,
                                            std::string_view confirmLabel);

} // namespace lucid::platform
