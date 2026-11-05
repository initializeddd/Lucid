#include "modules/HexBuffer.hpp"
#include <limits>
int main() {
    using lucid::modules::HexBuffer;
    HexBuffer b(8, 4); if (!b.load(std::string_view("a\0bc", 4)) || b.load("123456789") || b.bytes().size() != 4) return 1;
    if (!b.select(1, 3) || b.select(std::numeric_limits<std::size_t>::max(), 1) || b.select(3, 2) || b.selectionOffset() != 1) return 2;
    if (b.find(std::string_view("\0b", 2)) != 1 || b.find("", 0) != HexBuffer::notFound || b.find("a", 5) != HexBuffer::notFound) return 3;
    if (!b.overwrite(0, "XY") || b.bytes().substr(0, 2) != "XY" || !b.undo() || b.bytes()[0] != 'a' || !b.redo()) return 4;
    if (!b.overwrite(2, "Z") || b.undo() == false || !b.overwrite(3, "Q") || b.redo()) return 5;
    if (b.overwrite(3, "xx") || b.overwrite(0, "123") || b.overwrite(0, "")) return 6;
    auto view = std::string_view(b.bytes()).substr(0, 2); if (!b.overwrite(2, view) || b.bytes().substr(2) != "XY") return 7;
    if (!b.load("ok") || b.undo() || b.selectionLength()) return 8;
    if (!b.setCaret(1) || !b.moveCaret(1, true) || b.selectionOffset() != 1 || b.selectionLength() != 1 || b.caret() != 2) return 9;
    if (!b.moveCaret(-2, true) || b.selectionOffset() != 0 || b.selectionLength() != 1 || b.moveCaret(std::numeric_limits<std::ptrdiff_t>::min(), true)) return 10;
    if (!b.setCaret(0) || b.selectionLength() || b.setCaret(3)) return 11;
    if (!b.overwriteNibble(0, true, 15) || static_cast<unsigned char>(b.bytes()[0]) != 0xFF || !b.undo() || b.bytes()[0] != 'o') return 12;
    if (!b.overwriteNibble(0, false, 0) || static_cast<unsigned char>(b.bytes()[0]) != 0x60 || b.overwriteNibble(2, true, 1) || b.overwriteNibble(0, true, 16)) return 13;
    if (HexBuffer::formatByte(0x0a) != "0A" || HexBuffer::formatByte(0xff) != "FF") return 14;
    HexBuffer edits(5, 8); if (!edits.load("abc") || !edits.insert(1, "XY") || edits.bytes() != "aXYbc" || edits.caret() != 3) return 15;
    if (edits.insert(0, "z") || edits.erase(4, 2) || edits.erase(0, 0) || edits.insert(0, "")) return 16;
    if (!edits.undo() || edits.bytes() != "abc" || edits.caret() != 1 || !edits.redo() || edits.bytes() != "aXYbc") return 17;
    if (!edits.select(1, 3) || !edits.erase(1, 3) || edits.bytes() != "ac" || edits.selectionLength() || edits.caret() != 1 || !edits.undo() || edits.bytes() != "aXYbc") return 18;
    auto alias = std::string_view(edits.bytes()).substr(1, 2); if (!edits.erase(0, 2) || !edits.insert(0, std::string_view(edits.bytes()).substr(0, 2)) || edits.bytes() != "YbYbc") return 19;
    (void)alias;
    HexBuffer search; const std::string bytes("ababa\0ababa", 11); if (!search.load(bytes)) return 20;
    for (const std::string& needle : {std::string("aba"), std::string("ba\0a",4), std::string("a"), std::string("missing")}) {
        for (std::size_t start=0; start<=bytes.size()+1; ++start) {
            if (search.find(needle,start)!=bytes.find(needle,start) || search.rfind(needle,start)!=bytes.rfind(needle,start)) return 21;
        }
        if (search.rfind(needle)!=bytes.rfind(needle)) return 22;
    }
    if (search.rfind("")!=HexBuffer::notFound) return 23;
    std::string repetitive(100000,'a'); repetitive += 'b';
    if (!search.load(repetitive) || search.find(std::string(50000,'a')+'b')!=50000) return 24;
    lucid::modules::HexIntegerInterpretation number;
    HexBuffer values; if(!values.load(std::string("\x01\x80\x00\x00\x00\x00\x00\x80",8))) return 25;
    if(values.interpretSelection(number) || !values.select(0,2) || !values.interpretSelection(number) || number.littleUnsigned!="32769" || number.littleSigned!="-32767" || number.bigUnsigned!="384" || number.bigSigned!="384") return 26;
    if(!values.select(0,8) || !values.interpretSelection(number) || number.littleUnsigned!="9223372036854808577" || number.littleSigned!="-9223372036854743039") return 27;
    if(!values.select(1,1) || !values.interpretSelection(number) || number.littleSigned!="-128" || number.bigUnsigned!="128") return 28;
    if(!values.select(0,3) || values.interpretSelection(number) || !number.littleSigned.empty()) return 29;
    if(!values.load(std::string("\x00\x00\x00\x00\x00\x00\x00\x80",8)) || !values.select(0,8) || !values.interpretSelection(number) || number.littleSigned!="-9223372036854775808") return 30;
    return 0;
}
