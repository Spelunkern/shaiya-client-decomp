#include "config/development_version.h"
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <string>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
void set(const char* value) {
#ifdef _WIN32
    SetEnvironmentVariableA("SHAIYA_CLIENT_VERSION",value);
#else
    if(value) setenv("SHAIYA_CLIENT_VERSION",value,1);else unsetenv("SHAIYA_CLIENT_VERSION");
#endif
}
int main() {
    struct Case {const char* text;std::uint32_t expected;};
    const Case cases[]={{nullptr,0x11122200},{"",0x11122200},{"0",0},{"00123",123},{"0xA1",161},
        {"4294967295",0xffffffff},{"4294967296",0x11122200},{"0x100000000",0x11122200},
        {"-1",0x11122200},{"+1",0x11122200},{" 1",0x11122200},{"1 ",0x11122200},{"5.4",0x11122200},{"0x",0x11122200}};
    for(const auto& test:cases) {set(test.text);shaiya_apply_development_client_version();if(shaiya_client_version!=test.expected) {std::fputs("Native version override parity failed\n",stderr);return 1;}}
    std::puts("Native client-version configuration parity passed");return 0;
}
