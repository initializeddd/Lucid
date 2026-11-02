#include "app/Settings.hpp"

#include "editor/json/Json.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <utility>

namespace lucid {
namespace {

[[nodiscard]] std::filesystem::path homeDirectory() {
    if (const char* home = std::getenv("HOME"); home != nullptr) {
        return std::filesystem::path(home);
    }
    std::error_code error;
    const std::filesystem::path current = std::filesystem::current_path(error);
    return error ? std::filesystem::path{} : current;
}

[[nodiscard]] std::string withLocation(std::string message,
                                       const editor::JsonError& error) {
    return "Line " + std::to_string(error.line + 1) + ", column " +
           std::to_string(error.column) + ": " + std::move(message);
}

[[nodiscard]] int hexValue(char character) {
    if (character >= '0' && character <= '9') {
        return character - '0';
    }
    if (character >= 'a' && character <= 'f') {
        return character - 'a' + 10;
    }
    if (character >= 'A' && character <= 'F') {
        return character - 'A' + 10;
    }
    return -1;
}

[[nodiscard]] bool parseColor(std::string_view text, ui::theme::Color& color) {
    if (text.size() != 7 || text.front() != '#') {
        return false;
    }
    std::array<int, 6> digits{};
    for (std::size_t index = 0; index < digits.size(); ++index) {
        digits[index] = hexValue(text[index + 1]);
        if (digits[index] < 0) {
            return false;
        }
    }
    const int red = digits[0] * 16 + digits[1];
    const int green = digits[2] * 16 + digits[3];
    const int blue = digits[4] * 16 + digits[5];
    color = ui::theme::Color{static_cast<float>(red) / 255.0F,
                             static_cast<float>(green) / 255.0F,
                             static_cast<float>(blue) / 255.0F, 1.0F};
    return true;
}

[[nodiscard]] std::string colorToHex(const ui::theme::Color& color) {
    const auto channel = [](float value) {
        const int clamped =
            static_cast<int>((value < 0.0F ? 0.0F : value > 1.0F ? 1.0F : value) *
                             255.0F + 0.5F);
        return std::clamp(clamped, 0, 255);
    };
    std::ostringstream stream;
    stream << '#' << std::hex << std::setfill('0') << std::nouppercase
           << std::setw(2) << channel(color.r) << std::setw(2) << channel(color.g)
           << std::setw(2) << channel(color.b);
    return stream.str();
}

} // namespace

SettingsResult parseSettings(std::string_view text) {
    SettingsResult result;
    editor::JsonError parseError;
    const std::optional<editor::JsonValue> root =
        editor::parseJson(text, parseError);
    if (!root.has_value()) {
        result.error = withLocation(parseError.message, parseError);
        return result;
    }
    if (root->type != editor::JsonType::Object) {
        result.error = "Settings must be a JSON object";
        return result;
    }

    EditorSettings settings;
    for (const editor::JsonValue& member : root->children) {
        if (member.key == "animationSpeed") {
            if (member.type != editor::JsonType::Number) {
                result.error = "animationSpeed must be a number";
                return result;
            }
            const double speed = member.numberValue;
            if (speed != 0.5 && speed != 1.0 && speed != 2.0) {
                result.error = "animationSpeed must be 0.5, 1, or 2";
                return result;
            }
            settings.animationSpeed = static_cast<float>(speed);
        } else if (member.key == "showSyntaxErrors") {
            if (member.type != editor::JsonType::Bool) {
                result.error = "showSyntaxErrors must be true or false";
                return result;
            }
            settings.showSyntaxErrors = member.boolValue;
        } else if (member.key == "showLineNumbers") {
            if (member.type != editor::JsonType::Bool) {
                result.error = "showLineNumbers must be true or false";
                return result;
            }
            settings.showLineNumbers = member.boolValue;
        } else if (member.key == "highlightCurrentLine") {
            if (member.type != editor::JsonType::Bool) {
                result.error = "highlightCurrentLine must be true or false";
                return result;
            }
            settings.highlightCurrentLine = member.boolValue;
        } else if (member.key == "cursorBlink") {
            if (member.type != editor::JsonType::Bool) {
                result.error = "cursorBlink must be true or false";
                return result;
            }
            settings.cursorBlink = member.boolValue;
        } else if (member.key == "cursorStyle") {
            if (member.type != editor::JsonType::String) {
                result.error = "cursorStyle must be a string";
                return result;
            }
            std::string style = member.stringValue;
            std::transform(style.begin(), style.end(), style.begin(),
                           [](unsigned char character) {
                               return static_cast<char>(std::tolower(character));
                           });
            if (style == "line" || style == "bar") {
                settings.cursorStyle = editor::CursorStyle::Line;
            } else if (style == "block") {
                settings.cursorStyle = editor::CursorStyle::Block;
            } else if (style == "underscore" || style == "underline") {
                settings.cursorStyle = editor::CursorStyle::Underscore;
            } else if (style == "kitty") {
                settings.cursorStyle = editor::CursorStyle::Kitty;
            } else {
                result.error =
                    "cursorStyle must be line, block, underscore, or kitty";
                return result;
            }
        } else if (member.key == "backgroundMotion") {
            if (member.type != editor::JsonType::Bool) {
                result.error = "backgroundMotion must be true or false";
                return result;
            }
            settings.backgroundMotion = member.boolValue;
        } else if (member.key == "zoomFactor") {
            if (member.type != editor::JsonType::Number) {
                result.error = "zoomFactor must be a number";
                return result;
            }
            const double zoom = member.numberValue;
            if (zoom < 0.5 || zoom > 3.0) {
                result.error = "zoomFactor must be between 0.5 and 3.0";
                return result;
            }
            settings.zoomFactor = static_cast<float>(zoom);
        } else if (member.key == "theme") {
            if (member.type != editor::JsonType::String) {
                result.error = "theme must be a string";
                return result;
            }
            settings.theme.name = member.stringValue;
        } else if (member.key == "primary") {
            if (member.type != editor::JsonType::String) {
                result.error = "primary must be a #RRGGBB string";
                return result;
            }
            ui::theme::Color primary;
            if (!parseColor(member.stringValue, primary)) {
                result.error = "primary must be a #RRGGBB color";
                return result;
            }
            settings.theme.primary = primary;
        } else if (member.key == "customTheme") {
            if (member.type != editor::JsonType::Object) {
                result.error = "customTheme must be an object";
                return result;
            }
            for (const editor::JsonValue& child : member.children) {
                if (child.key != "accent") {
                    continue;
                }
                if (child.type != editor::JsonType::String) {
                    result.error = "customTheme.accent must be a #RRGGBB string";
                    return result;
                }
                ui::theme::Color primary;
                if (!parseColor(child.stringValue, primary)) {
                    result.error = "customTheme.accent must be a #RRGGBB color";
                    return result;
                }
                settings.theme.primary = primary;
            }
        } else {
            result.error = "Unknown setting \"" + member.key + "\"";
            return result;
        }
    }

    std::string themeName = settings.theme.name;
    std::transform(themeName.begin(), themeName.end(), themeName.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
    if (themeName.empty() || themeName == "dark" || themeName == "midnight" ||
        themeName == "custom") {
        themeName = "lucid-dark";
    } else if (themeName == "light") {
        themeName = "lucid-light";
    }
    settings.theme.name = themeName;

    ui::theme::Palette ignored;
    if (!ui::theme::paletteByName(settings.theme.name, ignored)) {
        result.error = "theme must be lucid-dark, graphite, or lucid-light";
        return result;
    }

    result.settings = settings;
    result.valid = true;
    return result;
}

std::string defaultSettingsJson() {
    return "{\n"
           "    \"animationSpeed\": 1.0,\n"
           "    \"showSyntaxErrors\": true,\n"
           "    \"showLineNumbers\": true,\n"
           "    \"highlightCurrentLine\": true,\n"
           "    \"cursorBlink\": true,\n"
           "    \"cursorStyle\": \"line\",\n"
           "    \"backgroundMotion\": true,\n"
           "    \"zoomFactor\": 1.0,\n"
           "    \"theme\": \"lucid-dark\"\n"
           "}\n";
}

std::string serializeSettings(const EditorSettings& settings) {
    std::ostringstream stream;
    stream << "{\n";
    stream << "    \"animationSpeed\": " << std::fixed << std::setprecision(1)
           << settings.animationSpeed << ",\n";
    stream << "    \"showSyntaxErrors\": "
           << (settings.showSyntaxErrors ? "true" : "false") << ",\n";
    stream << "    \"showLineNumbers\": "
           << (settings.showLineNumbers ? "true" : "false") << ",\n";
    stream << "    \"highlightCurrentLine\": "
           << (settings.highlightCurrentLine ? "true" : "false") << ",\n";
    stream << "    \"cursorBlink\": "
           << (settings.cursorBlink ? "true" : "false") << ",\n";
    stream << "    \"cursorStyle\": \"" << editor::cursorStyleName(settings.cursorStyle)
           << "\",\n";
    stream << "    \"backgroundMotion\": "
           << (settings.backgroundMotion ? "true" : "false") << ",\n";
    stream << "    \"zoomFactor\": " << std::fixed << std::setprecision(2)
           << settings.zoomFactor << ",\n";
    stream << "    \"theme\": \"" << settings.theme.name << "\"";
    if (settings.theme.primary.has_value()) {
        stream << ",\n    \"primary\": \""
               << colorToHex(*settings.theme.primary) << "\"";
    }
    stream << "\n}\n";
    return stream.str();
}

std::filesystem::path settingsDirectory() {
#if defined(__APPLE__)
    const std::filesystem::path home = homeDirectory();
    if (home.empty()) {
        return {};
    }
    return home / "Library" / "Application Support" / "Lucid" / "Settings";
#elif defined(_WIN32)
    if (const char* appData = std::getenv("APPDATA"); appData != nullptr) {
        return std::filesystem::path(appData) / "Lucid" / "Settings";
    }
    return {};
#else
    if (const char* config = std::getenv("XDG_CONFIG_HOME");
        config != nullptr && config[0] != '\0') {
        return std::filesystem::path(config) / "Lucid" / "Settings";
    }
    const std::filesystem::path home = homeDirectory();
    if (home.empty()) {
        return {};
    }
    return home / ".config" / "Lucid" / "Settings";
#endif
}

} // namespace lucid
