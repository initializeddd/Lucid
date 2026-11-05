#pragma once
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
namespace lucid::modules {
class HexBuffer {
public:
    explicit HexBuffer(std::size_t maxBytes = 16 * 1024 * 1024, std::size_t maxHistoryBytes = 4 * 1024 * 1024);
    [[nodiscard]] bool load(std::string_view bytes);
    [[nodiscard]] const std::string& bytes() const { return bytes_; }
    [[nodiscard]] bool select(std::size_t offset, std::size_t length);
    [[nodiscard]] std::size_t selectionOffset() const { return offset_; }
    [[nodiscard]] std::size_t selectionLength() const { return length_; }
    // Caret is a byte boundary in [0, size]; extending selects between anchor and caret.
    [[nodiscard]] bool moveCaret(std::ptrdiff_t delta, bool extend = false);
    [[nodiscard]] bool setCaret(std::size_t position, bool extend = false);
    [[nodiscard]] std::size_t caret() const { return caret_; }
    [[nodiscard]] bool overwriteNibble(std::size_t offset, bool high, unsigned value);
    [[nodiscard]] static std::string formatByte(unsigned char value);
    [[nodiscard]] std::size_t find(std::string_view needle, std::size_t start = 0) const;
    // Finds the last match whose starting offset is at most start.
    [[nodiscard]] std::size_t rfind(std::string_view needle, std::size_t start = std::string::npos) const;
    [[nodiscard]] bool overwrite(std::size_t offset, std::string_view bytes);
    [[nodiscard]] bool insert(std::size_t offset, std::string_view bytes);
    [[nodiscard]] bool erase(std::size_t offset, std::size_t length);
    [[nodiscard]] bool undo();
    [[nodiscard]] bool redo();
    static constexpr std::size_t notFound = std::string::npos;
private:
    [[nodiscard]] bool applyEdit(std::size_t offset, std::size_t remove, std::string_view insert);
    struct Edit { std::size_t offset; std::string before, after; };
    std::string bytes_; std::size_t offset_ = 0, length_ = 0, maxBytes_, maxHistoryBytes_, historyBytes_ = 0, cursor_ = 0, caret_ = 0, anchor_ = 0;
    std::vector<Edit> history_;
};
}
