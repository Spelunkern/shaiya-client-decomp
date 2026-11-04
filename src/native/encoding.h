#pragma once
#include <string>
#include <string_view>
namespace genesis {
// The recovered English client uses CP1252 wire strings. Keep these byte
// contracts separate from SDL's UTF-8 input and FreeType's Unicode glyphs.
std::string cp1252_to_utf8(std::string_view bytes);
std::string utf8_to_cp1252(std::string_view utf8);
void erase_last_utf8(std::string& text);
}
