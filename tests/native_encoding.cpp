#include "encoding.h"
#include <cstdio>
#include <stdexcept>
int main() {
    try {
        const std::string utf8="España € “Sí”",bytes="Espa\xf1\x61 \x80 \x93S\xed\x94";
        if(genesis::utf8_to_cp1252(utf8)!=bytes || genesis::cp1252_to_utf8(bytes)!=utf8) throw std::runtime_error("CP1252/UTF-8 text contract");
        for(unsigned i=0;i<256;++i) {std::string byte(1,static_cast<char>(i));if(genesis::utf8_to_cp1252(genesis::cp1252_to_utf8(byte))!=byte) throw std::runtime_error("CP1252 byte round-trip");}
        std::string text="José€";genesis::erase_last_utf8(text);if(text!="José") return 1;genesis::erase_last_utf8(text);if(text!="Jos") return 1;
        try {(void)genesis::utf8_to_cp1252("😀");throw std::logic_error("Accepted unsupported wire character");}catch(const std::invalid_argument&) {}
        std::puts("Native wire/UI encoding checks passed");return 0;
    }catch(const std::exception& e) {std::fprintf(stderr,"%s\n",e.what());return 1;}
}
