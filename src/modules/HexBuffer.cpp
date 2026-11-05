#include "modules/HexBuffer.hpp"
#include <algorithm>
#include <cstdint>
#include <limits>
namespace lucid::modules {
HexBuffer::HexBuffer(std::size_t maxBytes, std::size_t maxHistoryBytes) : maxBytes_(maxBytes), maxHistoryBytes_(maxHistoryBytes) {}
bool HexBuffer::load(std::string_view bytes) {
    if (bytes.size() > maxBytes_) return false;
    // Copy before replacing to allow a view into the current buffer.
    std::string copy(bytes); bytes_ = std::move(copy); offset_ = length_ = historyBytes_ = cursor_ = caret_ = anchor_ = 0; history_.clear(); return true;
}
bool HexBuffer::select(std::size_t offset, std::size_t length) {
    if (offset > bytes_.size() || length > bytes_.size() - offset) return false;
    offset_ = offset; length_ = length; anchor_ = offset; caret_ = offset + length; return true;
}
bool HexBuffer::interpretSelection(HexIntegerInterpretation& result) const {
    result = {};
    if (length_ != 1 && length_ != 2 && length_ != 4 && length_ != 8) return false;
    if (offset_ > bytes_.size() || length_ > bytes_.size() - offset_) return false;
    std::uint64_t little=0, big=0;
    for(std::size_t i=0;i<length_;++i) {
        const auto byte=static_cast<std::uint8_t>(bytes_[offset_+i]);
        little |= static_cast<std::uint64_t>(byte) << (8*i);
        big=(big<<8) | byte;
    }
    const auto signedText=[this](std::uint64_t value) {
        const auto bits=length_*8;
        if ((value & (std::uint64_t{1} << (bits-1))) == 0) return std::to_string(value);
        // Compute magnitude without signed overflow or implementation-defined casts.
        const auto mask=bits==64 ? std::numeric_limits<std::uint64_t>::max() : (std::uint64_t{1} << bits)-1;
        return std::string("-")+std::to_string(((~value)&mask)+1);
    };
    result.littleUnsigned=std::to_string(little); result.littleSigned=signedText(little);
    result.bigUnsigned=std::to_string(big); result.bigSigned=signedText(big);
    return true;
}
bool HexBuffer::setCaret(std::size_t position, bool extend) {
    if (position > bytes_.size()) return false;
    if (!extend) anchor_ = position;
    caret_ = position; offset_ = std::min(anchor_, caret_); length_ = std::max(anchor_, caret_) - offset_; return true;
}
bool HexBuffer::moveCaret(std::ptrdiff_t delta, bool extend) {
    // Avoid negating PTRDIFF_MIN and avoid unsigned overflow.
    if (delta < 0) {
        auto distance = static_cast<std::size_t>(-(delta + 1)) + 1;
        if (distance > caret_) return false;
        return setCaret(caret_ - distance, extend);
    }
    auto distance = static_cast<std::size_t>(delta);
    if (distance > bytes_.size() - caret_) return false;
    return setCaret(caret_ + distance, extend);
}
bool HexBuffer::overwriteNibble(std::size_t offset, bool high, unsigned value) {
    if (offset >= bytes_.size() || value > 15) return false;
    auto byte = static_cast<unsigned char>(bytes_[offset]);
    char replacement = static_cast<char>(high ? ((byte & 0x0f) | (value << 4)) : ((byte & 0xf0) | value));
    return overwrite(offset, std::string_view(&replacement, 1));
}
std::string HexBuffer::formatByte(unsigned char value) {
    constexpr char digits[] = "0123456789ABCDEF";
    return {digits[value >> 4], digits[value & 15]};
}
namespace {
template<typename Index>
std::size_t searchBytes(std::string_view bytes, std::string_view needle, std::size_t first, std::size_t last, bool reverse) {
    std::vector<Index> prefix(needle.size());
    for (std::size_t i=1, matched=0;i<needle.size();++i) {
        while (matched && needle[i]!=needle[matched]) matched=prefix[matched-1];
        if (needle[i]==needle[matched]) ++matched;
        prefix[i]=static_cast<Index>(matched);
    }
    std::size_t found=std::string::npos;
    for (std::size_t i=first, matched=0;i<last+needle.size();++i) {
        while (matched && bytes[i]!=needle[matched]) matched=prefix[matched-1];
        if (bytes[i]==needle[matched]) ++matched;
        if (matched==needle.size()) {
            found=i+1-needle.size();
            if (!reverse) return found;
            matched=prefix[matched-1];
        }
    }
    return found;
}
std::size_t boundedSearch(std::string_view bytes, std::string_view needle, std::size_t first, std::size_t last, bool reverse) {
    if (needle.size() <= std::numeric_limits<std::uint32_t>::max()) return searchBytes<std::uint32_t>(bytes,needle,first,last,reverse);
    return searchBytes<std::size_t>(bytes,needle,first,last,reverse);
}
}
std::size_t HexBuffer::find(std::string_view needle, std::size_t start) const {
    if (needle.empty() || start > bytes_.size() || needle.size() > bytes_.size()-start) return notFound;
    return boundedSearch(bytes_,needle,start,bytes_.size()-needle.size(),false);
}
std::size_t HexBuffer::rfind(std::string_view needle, std::size_t start) const {
    if (needle.empty() || needle.size() > bytes_.size()) return notFound;
    return boundedSearch(bytes_,needle,0,std::min(start,bytes_.size()-needle.size()),true);
}
bool HexBuffer::overwrite(std::size_t offset, std::string_view bytes) {
    if (bytes.empty()) return false;
    return applyEdit(offset, bytes.size(), bytes);
}
bool HexBuffer::insert(std::size_t offset, std::string_view bytes) {
    if (bytes.empty()) return false;
    return applyEdit(offset, 0, bytes);
}
bool HexBuffer::erase(std::size_t offset, std::size_t length) {
    if (!length) return false;
    return applyEdit(offset, length, {});
}
bool HexBuffer::applyEdit(std::size_t offset, std::size_t remove, std::string_view insert) {
    if (offset > bytes_.size() || remove > bytes_.size() - offset || insert.size() > maxBytes_ - (bytes_.size() - remove) ||
        remove > maxHistoryBytes_ || insert.size() > maxHistoryBytes_ - remove) return false;
    Edit edit{offset, bytes_.substr(offset, remove), std::string(insert)};
    if (edit.before == edit.after) return true;
    while (history_.size() > cursor_) { historyBytes_ -= history_.back().before.size() + history_.back().after.size(); history_.pop_back(); }
    const auto cost = edit.before.size() + edit.after.size();
    while (!history_.empty() && (historyBytes_ > maxHistoryBytes_ - cost || history_.size() >= 10000)) {
        historyBytes_ -= history_.front().before.size() + history_.front().after.size(); history_.erase(history_.begin()); --cursor_;
    }
    bytes_.replace(offset, remove, edit.after); historyBytes_ += cost; history_.push_back(std::move(edit)); ++cursor_;
    (void)setCaret(offset + insert.size()); return true;
}
bool HexBuffer::undo() {
    if (!cursor_) return false; const auto& e = history_[--cursor_]; bytes_.replace(e.offset, e.after.size(), e.before);
    (void)setCaret(e.offset + e.before.size()); return true;
}
bool HexBuffer::redo() {
    if (cursor_ == history_.size()) return false; const auto& e = history_[cursor_++]; bytes_.replace(e.offset, e.before.size(), e.after);
    (void)setCaret(e.offset + e.after.size()); return true;
}
}
