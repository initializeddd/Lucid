#pragma once

#include "input/KeyCodes.hpp"
#include "input/MouseCodes.hpp"

#include <vulkan/vulkan.h>

#include <functional>
#include <string>
#include <string_view>
#include <vector>

struct GLFWwindow;
struct GLFWcursor;

namespace lucid {

struct WindowConfig {
    int width = 1280;
    int height = 820;
    int minWidth = 640;
    int minHeight = 400;
    std::string title = "Lucid";
    bool resizable = true;
};

enum class CursorShape {
    Arrow,
    IBeam,
    Hand,
};

class Window {
public:
    using KeyCallback = std::function<void(const input::KeyEvent&)>;
    using CharCallback = std::function<void(char32_t)>;
    using MouseButtonCallback = std::function<void(const input::MouseButtonEvent&)>;
    using MouseMoveCallback = std::function<void(const input::MouseMoveEvent&)>;
    using DropCallback = std::function<void(std::vector<std::string>)>;
    using ScrollCallback = std::function<void(const input::ScrollEvent&)>;

    explicit Window(const WindowConfig& config);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) = delete;
    Window& operator=(Window&&) = delete;

    [[nodiscard]] bool shouldClose() const noexcept;
    void pollEvents() const;

    [[nodiscard]] GLFWwindow* handle() const noexcept { return m_window; }
    void framebufferSize(int& width, int& height) const noexcept;
    void windowSize(int& width, int& height) const noexcept;
    [[nodiscard]] float contentScale() const noexcept;
    void setTitle(std::string_view title);
    void show();
    void focus();
    void setCursorShape(CursorShape shape) noexcept;

    void beginWindowDrag() noexcept;
    void updateWindowDrag() noexcept;
    void endWindowDrag() noexcept;
    [[nodiscard]] bool isDraggingWindow() const noexcept { return m_draggingWindow; }

    [[nodiscard]] std::string clipboardText() const;
    void setClipboardText(std::string_view text);

    void setKeyCallback(KeyCallback callback);
    void setCharCallback(CharCallback callback);
    void setMouseButtonCallback(MouseButtonCallback callback);
    void setMouseMoveCallback(MouseMoveCallback callback);
    void setScrollCallback(ScrollCallback callback);
    void setDropCallback(DropCallback callback);

    [[nodiscard]] VkSurfaceKHR createSurface(VkInstance instance) const;
    [[nodiscard]] static std::vector<const char*> requiredVulkanInstanceExtensions();

private:
    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void charCallback(GLFWwindow* window, unsigned int codepoint);
    static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void cursorPosCallback(GLFWwindow* window, double x, double y);
    static void dropCallback(GLFWwindow* window, int count, const char** paths);
    static void scrollCallback(GLFWwindow* window, double xOffset, double yOffset);

    GLFWwindow* m_window = nullptr;
    GLFWcursor* m_arrowCursor = nullptr;
    GLFWcursor* m_ibeamCursor = nullptr;
    GLFWcursor* m_handCursor = nullptr;
    CursorShape m_cursorShape = CursorShape::Arrow;
    bool m_draggingWindow = false;
    double m_dragStartScreenX = 0.0;
    double m_dragStartScreenY = 0.0;
    int m_dragStartWindowX = 0;
    int m_dragStartWindowY = 0;
    KeyCallback m_keyCallback;
    CharCallback m_charCallback;
    MouseButtonCallback m_mouseButtonCallback;
    MouseMoveCallback m_mouseMoveCallback;
    ScrollCallback m_scrollCallback;
    DropCallback m_dropCallback;
};

} // namespace lucid
