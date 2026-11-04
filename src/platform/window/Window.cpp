#include "platform/window/Window.hpp"

#include "logging/Log.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>

namespace lucid {
namespace {

int g_windowCount = 0;

void glfwErrorCallback(int code, const char* description) {
    log::error(std::string("GLFW error ") + std::to_string(code) + ": " +
               (description != nullptr ? description : "unknown error"));
}

void centerOnPrimaryMonitor(GLFWwindow* window) {
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    if (monitor == nullptr) {
        return;
    }

    int areaX = 0;
    int areaY = 0;
    int areaWidth = 0;
    int areaHeight = 0;
    glfwGetMonitorWorkarea(monitor, &areaX, &areaY, &areaWidth, &areaHeight);

    int windowWidth = 0;
    int windowHeight = 0;
    glfwGetWindowSize(window, &windowWidth, &windowHeight);

    glfwSetWindowPos(window, areaX + (areaWidth - windowWidth) / 2,
                     areaY + (areaHeight - windowHeight) / 2);
}

[[nodiscard]] input::Key translateKey(int key) noexcept {
    switch (key) {
    case GLFW_KEY_ESCAPE:
        return input::Key::Escape;
    case GLFW_KEY_BACKSPACE:
        return input::Key::Backspace;
    case GLFW_KEY_ENTER:
    case GLFW_KEY_KP_ENTER:
        return input::Key::Enter;
    case GLFW_KEY_SPACE:
        return input::Key::Space;
    case GLFW_KEY_TAB:
        return input::Key::Tab;
    case GLFW_KEY_DELETE:
        return input::Key::Delete;
    case GLFW_KEY_LEFT:
        return input::Key::Left;
    case GLFW_KEY_RIGHT:
        return input::Key::Right;
    case GLFW_KEY_UP:
        return input::Key::Up;
    case GLFW_KEY_DOWN:
        return input::Key::Down;
    case GLFW_KEY_HOME:
        return input::Key::Home;
    case GLFW_KEY_END:
        return input::Key::End;
    case GLFW_KEY_F8:
        return input::Key::F8;
    case GLFW_KEY_F12:
        return input::Key::F12;
    case GLFW_KEY_B: return input::Key::B;
    case GLFW_KEY_A:
        return input::Key::A;
    case GLFW_KEY_C:
        return input::Key::C;
    case GLFW_KEY_EQUAL: return input::Key::Equal;
    case GLFW_KEY_MINUS: return input::Key::Minus;
    case GLFW_KEY_0: return input::Key::Zero;
    case GLFW_KEY_U: return input::Key::U;
    case GLFW_KEY_COMMA:
        return input::Key::Comma;
    case GLFW_KEY_H:
        return input::Key::H;
    case GLFW_KEY_J:
        return input::Key::J;
    case GLFW_KEY_E:
        return input::Key::E;
    case GLFW_KEY_G: return input::Key::G;
    case GLFW_KEY_F:
        return input::Key::F;
    case GLFW_KEY_D:
        return input::Key::D;
    case GLFW_KEY_M: return input::Key::M;
    case GLFW_KEY_L: return input::Key::L;
    case GLFW_KEY_K:
        return input::Key::K;
    case GLFW_KEY_N:
        return input::Key::N;
    case GLFW_KEY_O:
        return input::Key::O;
    case GLFW_KEY_P:
        return input::Key::P;
    case GLFW_KEY_R:
        return input::Key::R;
    case GLFW_KEY_T:
        return input::Key::T;
    case GLFW_KEY_S:
        return input::Key::S;
    case GLFW_KEY_SLASH:
        return input::Key::Slash;
    case GLFW_KEY_V:
        return input::Key::V;
    case GLFW_KEY_W:
        return input::Key::W;
    case GLFW_KEY_X:
        return input::Key::X;
    case GLFW_KEY_Y:
        return input::Key::Y;
    case GLFW_KEY_Z:
        return input::Key::Z;
    default:
        return input::Key::Unknown;
    }
}

[[nodiscard]] input::KeyAction translateAction(int action) noexcept {
    switch (action) {
    case GLFW_PRESS:
        return input::KeyAction::Press;
    case GLFW_REPEAT:
        return input::KeyAction::Repeat;
    default:
        return input::KeyAction::Release;
    }
}

[[nodiscard]] input::MouseButton translateMouseButton(int button) noexcept {
    switch (button) {
    case GLFW_MOUSE_BUTTON_LEFT:
        return input::MouseButton::Left;
    case GLFW_MOUSE_BUTTON_RIGHT:
        return input::MouseButton::Right;
    case GLFW_MOUSE_BUTTON_MIDDLE:
        return input::MouseButton::Middle;
    default:
        return input::MouseButton::Unknown;
    }
}

} // namespace

Window::Window(const WindowConfig& config) {
    if (g_windowCount == 0) {
        glfwSetErrorCallback(glfwErrorCallback);
        if (glfwInit() != GLFW_TRUE) {
            throw std::runtime_error("Failed to initialize GLFW");
        }
        if (glfwVulkanSupported() != GLFW_TRUE) {
            glfwTerminate();
            throw std::runtime_error(
                "Vulkan is not available. Install a Vulkan loader and MoltenVK on macOS.");
        }
    }

    ++g_windowCount;

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, config.resizable ? GLFW_TRUE : GLFW_FALSE);

    m_window = glfwCreateWindow(config.width, config.height, config.title.c_str(), nullptr, nullptr);
    if (m_window == nullptr) {
        --g_windowCount;
        if (g_windowCount == 0) {
            glfwTerminate();
        }
        throw std::runtime_error("Failed to create the Lucid window");
    }

    glfwSetWindowSizeLimits(m_window, config.minWidth, config.minHeight,
                            GLFW_DONT_CARE, GLFW_DONT_CARE);
    glfwSetWindowUserPointer(m_window, this);
    glfwSetKeyCallback(m_window, &Window::keyCallback);
    glfwSetCharCallback(m_window, &Window::charCallback);
    glfwSetMouseButtonCallback(m_window, &Window::mouseButtonCallback);
    glfwSetCursorPosCallback(m_window, &Window::cursorPosCallback);
    glfwSetScrollCallback(m_window, &Window::scrollCallback);
    glfwSetDropCallback(m_window, &Window::dropCallback);

    m_arrowCursor = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
    m_ibeamCursor = glfwCreateStandardCursor(GLFW_IBEAM_CURSOR);
    m_handCursor = glfwCreateStandardCursor(GLFW_HAND_CURSOR);
    centerOnPrimaryMonitor(m_window);
}

Window::~Window() {
    if (m_arrowCursor != nullptr) {
        glfwDestroyCursor(m_arrowCursor);
        m_arrowCursor = nullptr;
    }
    if (m_ibeamCursor != nullptr) {
        glfwDestroyCursor(m_ibeamCursor);
        m_ibeamCursor = nullptr;
    }
    if (m_handCursor != nullptr) {
        glfwDestroyCursor(m_handCursor);
        m_handCursor = nullptr;
    }

    if (m_window != nullptr) {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }

    --g_windowCount;
    if (g_windowCount <= 0) {
        glfwTerminate();
        g_windowCount = 0;
    }
}

bool Window::shouldClose() const noexcept {
    return glfwWindowShouldClose(m_window) == GLFW_TRUE;
}

void Window::cancelClose() noexcept {
    glfwSetWindowShouldClose(m_window, GLFW_FALSE);
}

void Window::pollEvents() const {
    glfwPollEvents();
}

void Window::framebufferSize(int& width, int& height) const noexcept {
    glfwGetFramebufferSize(m_window, &width, &height);
}

void Window::windowSize(int& width, int& height) const noexcept {
    glfwGetWindowSize(m_window, &width, &height);
}

float Window::contentScale() const noexcept {
    int framebufferWidth = 0;
    int framebufferHeight = 0;
    int windowWidth = 0;
    int windowHeight = 0;
    glfwGetFramebufferSize(m_window, &framebufferWidth, &framebufferHeight);
    glfwGetWindowSize(m_window, &windowWidth, &windowHeight);
    if (windowWidth > 0 && framebufferWidth > 0) {
        return static_cast<float>(framebufferWidth) /
               static_cast<float>(windowWidth);
    }

    float scaleX = 1.0F;
    float scaleY = 1.0F;
    glfwGetWindowContentScale(m_window, &scaleX, &scaleY);
    return scaleX > 0.0F ? scaleX : 1.0F;
}

void Window::setTitle(std::string_view title) {
    if (m_window == nullptr) {
        return;
    }
    glfwSetWindowTitle(m_window, std::string(title).c_str());
}

void Window::show() {
    if (m_window != nullptr) {
        glfwShowWindow(m_window);
    }
}

void Window::focus() {
    if (m_window == nullptr) {
        return;
    }
    if (glfwGetWindowAttrib(m_window, GLFW_ICONIFIED) == GLFW_TRUE) {
        glfwRestoreWindow(m_window);
    }
    glfwFocusWindow(m_window);
}

void Window::beginWindowDrag() noexcept {
    if (m_window == nullptr) {
        return;
    }
    int windowX = 0;
    int windowY = 0;
    double cursorX = 0.0;
    double cursorY = 0.0;
    glfwGetWindowPos(m_window, &windowX, &windowY);
    glfwGetCursorPos(m_window, &cursorX, &cursorY);
    m_draggingWindow = true;
    m_dragStartWindowX = windowX;
    m_dragStartWindowY = windowY;
    m_dragStartScreenX = static_cast<double>(windowX) + cursorX;
    m_dragStartScreenY = static_cast<double>(windowY) + cursorY;
}

void Window::updateWindowDrag() noexcept {
    if (!m_draggingWindow || m_window == nullptr) {
        return;
    }
    if (glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_RELEASE) {
        m_draggingWindow = false;
        return;
    }
    int windowX = 0;
    int windowY = 0;
    double cursorX = 0.0;
    double cursorY = 0.0;
    glfwGetWindowPos(m_window, &windowX, &windowY);
    glfwGetCursorPos(m_window, &cursorX, &cursorY);
    const double screenX = static_cast<double>(windowX) + cursorX;
    const double screenY = static_cast<double>(windowY) + cursorY;
    const int nextX = m_dragStartWindowX +
                      static_cast<int>(std::lround(screenX - m_dragStartScreenX));
    const int nextY = m_dragStartWindowY +
                      static_cast<int>(std::lround(screenY - m_dragStartScreenY));
    glfwSetWindowPos(m_window, nextX, nextY);
}

void Window::endWindowDrag() noexcept {
    m_draggingWindow = false;
}

void Window::setCursorShape(CursorShape shape) noexcept {
    if (shape == m_cursorShape || m_window == nullptr) {
        return;
    }
    m_cursorShape = shape;

    GLFWcursor* cursor = m_arrowCursor;
    switch (shape) {
    case CursorShape::IBeam:
        cursor = m_ibeamCursor;
        break;
    case CursorShape::Hand:
        cursor = m_handCursor;
        break;
    case CursorShape::Arrow:
    default:
        break;
    }
    glfwSetCursor(m_window, cursor);
}

std::string Window::clipboardText() const {
    const char* text = glfwGetClipboardString(m_window);
    return text != nullptr ? std::string(text) : std::string{};
}

void Window::setClipboardText(std::string_view text) {
    const std::string value(text);
    glfwSetClipboardString(m_window, value.c_str());
}

void Window::setKeyCallback(KeyCallback callback) {
    m_keyCallback = std::move(callback);
}

void Window::setCharCallback(CharCallback callback) {
    m_charCallback = std::move(callback);
}

void Window::setMouseButtonCallback(MouseButtonCallback callback) {
    m_mouseButtonCallback = std::move(callback);
}

void Window::setMouseMoveCallback(MouseMoveCallback callback) {
    m_mouseMoveCallback = std::move(callback);
}

void Window::setScrollCallback(ScrollCallback callback) {
    m_scrollCallback = std::move(callback);
}

void Window::setDropCallback(DropCallback callback) { m_dropCallback = std::move(callback); }

void Window::dropCallback(GLFWwindow* window, int count, const char** paths) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (!self || !self->m_dropCallback || count <= 0) return;
    std::vector<std::string> files;
    for (int i = 0; i < count; ++i) if (paths[i]) files.emplace_back(paths[i]);
    self->m_dropCallback(std::move(files));
}

VkSurfaceKHR Window::createSurface(VkInstance instance) const {
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    const VkResult result = glfwCreateWindowSurface(instance, m_window, nullptr, &surface);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create the Vulkan surface (VkResult " +
                                 std::to_string(static_cast<int>(result)) + ")");
    }
    return surface;
}

std::vector<const char*> Window::requiredVulkanInstanceExtensions() {
    std::uint32_t count = 0;
    const char** extensions = glfwGetRequiredInstanceExtensions(&count);
    if (extensions == nullptr || count == 0) {
        throw std::runtime_error("GLFW could not determine the required Vulkan extensions");
    }
    return std::vector<const char*>(extensions, extensions + count);
}

void Window::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (self == nullptr || !self->m_keyCallback) {
        return;
    }

    input::KeyEvent event;
    event.key = translateKey(key);
    // GLFW key codes describe US key positions; shortcut letters should follow
    // the active keyboard layout, just as typed text does.
    if ((mods & (GLFW_MOD_CONTROL | GLFW_MOD_SUPER)) != 0) {
        if (const char* name = glfwGetKeyName(key, scancode); name && name[0] && !name[1]) {
            const char c = name[0];
            if (c >= 'a' && c <= 'z') event.key = translateKey(GLFW_KEY_A + c - 'a');
            else if (c >= 'A' && c <= 'Z') event.key = translateKey(GLFW_KEY_A + c - 'A');
        }
    }
    event.action = translateAction(action);
    event.shift = (mods & GLFW_MOD_SHIFT) != 0;
    event.control = (mods & GLFW_MOD_CONTROL) != 0;
    event.alt = (mods & GLFW_MOD_ALT) != 0;
    event.super = (mods & GLFW_MOD_SUPER) != 0;
    self->m_keyCallback(event);
}

void Window::mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (self == nullptr || !self->m_mouseButtonCallback) {
        return;
    }

    input::MouseButtonEvent event;
    glfwGetCursorPos(window, &event.x, &event.y);
    event.button = translateMouseButton(button);
    event.action = action == GLFW_RELEASE ? input::KeyAction::Release
                                          : input::KeyAction::Press;
    event.shift = (mods & GLFW_MOD_SHIFT) != 0;
    event.control = (mods & GLFW_MOD_CONTROL) != 0;
    event.alt = (mods & GLFW_MOD_ALT) != 0;
    event.super = (mods & GLFW_MOD_SUPER) != 0;
    self->m_mouseButtonCallback(event);
}

void Window::cursorPosCallback(GLFWwindow* window, double x, double y) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (self == nullptr || !self->m_mouseMoveCallback) {
        return;
    }

    input::MouseMoveEvent event;
    event.x = x;
    event.y = y;
    self->m_mouseMoveCallback(event);
}

void Window::scrollCallback(GLFWwindow* window, double xOffset, double yOffset) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (self == nullptr || !self->m_scrollCallback) {
        return;
    }

    input::ScrollEvent event;
    event.xOffset = xOffset;
    event.yOffset = yOffset;
    self->m_scrollCallback(event);
}

void Window::charCallback(GLFWwindow* window, unsigned int codepoint) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (self == nullptr || !self->m_charCallback) {
        return;
    }

#if defined(__APPLE__)
    const bool commandDown =
        glfwGetKey(window, GLFW_KEY_LEFT_SUPER) == GLFW_PRESS ||
        glfwGetKey(window, GLFW_KEY_RIGHT_SUPER) == GLFW_PRESS;
#else
    const bool commandDown =
        glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
        glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;
#endif

    if (commandDown) {
        return;
    }

    self->m_charCallback(static_cast<char32_t>(codepoint));
}

} // namespace lucid
