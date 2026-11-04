#include "encoding.h"
#include "font.h"
#include <array>
#include <stdexcept>

namespace genesis {
namespace {
constexpr std::array<char32_t,32> extra={
    0x20ac,0x81,0x201a,0x192,0x201e,0x2026,0x2020,0x2021,
    0x2c6,0x2030,0x160,0x2039,0x152,0x8d,0x17d,0x8f,
    0x90,0x2018,0x2019,0x201c,0x201d,0x2022,0x2013,0x2014,
    0x2dc,0x2122,0x161,0x203a,0x153,0x9d,0x17e,0x178};
void append_utf8(std::string& out,char32_t point) {
    if(point<128) out.push_back(static_cast<char>(point));
    else if(point<2048) {out.push_back(static_cast<char>(0xc0|(point>>6)));out.push_back(static_cast<char>(0x80|(point&63)));}
    else {out.push_back(static_cast<char>(0xe0|(point>>12)));out.push_back(static_cast<char>(0x80|((point>>6)&63)));out.push_back(static_cast<char>(0x80|(point&63)));}
}
}
std::string cp1252_to_utf8(std::string_view bytes) {
    std::string result;
    for(unsigned char byte:bytes) append_utf8(result,byte>=0x80 && byte<=0x9f?extra[byte-0x80]:byte);
    return result;
}
std::string utf8_to_cp1252(std::string_view utf8) {
    std::string result;
    for(char32_t point:decode_utf8(utf8)) {
        if(point<0x80 || (point>=0xa0 && point<=0xff)) {result.push_back(static_cast<char>(point));continue;}
        bool found=false;
        for(unsigned i=0;i<extra.size();++i) if(extra[i]==point) {result.push_back(static_cast<char>(i+0x80));found=true;break;}
        if(!found) throw std::invalid_argument("Text is not representable in client CP1252 encoding");
    }
    return result;
}
void erase_last_utf8(std::string& text) {
    if(text.empty()) return;
    auto at=text.size()-1;while(at && (static_cast<unsigned char>(text[at])&0xc0)==0x80) --at;
    text.resize(at);
}
}
