#include "application.h"
#include "config/development_version.h"
#include <cstdlib>
#include <cstdio>

namespace {
unsigned number(const std::string& text,unsigned maximum) {
    if(text.empty() || text.find_first_not_of("0123456789")!=std::string::npos) throw std::invalid_argument("Expected a positive decimal number");
    auto value=std::stoul(text);if(!value || value>maximum) throw std::invalid_argument("Numeric argument is out of range");
    return static_cast<unsigned>(value);
}
}
int main(int argc,char** argv) {
    try {
        genesis::AppSettings settings;int frames=0;std::filesystem::path capture;
        shaiya_apply_development_client_version();settings.client_version=shaiya_client_version;
        if(const auto* host=std::getenv("SHAIYA_LOGIN_HOST")) settings.login_host=host;
        for(int i=1;i<argc;++i) {
            std::string arg=argv[i];
            if(arg=="--help") {
                std::puts("Genesis native client (migration in progress)\n"
                    "--assets <directory> --glow <0-2> --font <font-file> --login-host <IPv4>\n"
                    "--login-port <port> --world-port <port> --frames <count> --capture <new.ppm>\n"
                    "Account input is interactive; no password command-line option.\n"
                    "Full world/gameplay integration is still incomplete.");return 0;
            }
            if(i+1>=argc) throw std::invalid_argument("Missing argument value: "+arg);
            std::string value=argv[++i];
            if(arg=="--assets") settings.assets=value;
            else if(arg=="--font") settings.font=value;
            else if(arg=="--login-host") settings.login_host=value;
            else if(arg=="--login-port") settings.login_port=static_cast<std::uint16_t>(number(value,65535));
            else if(arg=="--world-port") settings.world_port=static_cast<std::uint16_t>(number(value,65535));
            else if(arg=="--frames") {frames=static_cast<int>(number(value,1000000));settings.hidden=true;}
            else if(arg=="--capture") capture=value;
            else if(arg=="--glow") settings.glow_quality=static_cast<unsigned>(number(value,2));
            else throw std::invalid_argument("Unknown argument: "+arg);
        }
        genesis::Application app(std::move(settings));int rendered=0;
        while(app.frame()) {
            if(frames && ++rendered>=frames) break;
            if(SDL_GetWindowFlags(app.renderer().window())&SDL_WINDOW_MINIMIZED) SDL_Delay(50);
        }
        if(!capture.empty()) app.capture(capture);return 0;
    }catch(const std::exception& e) {std::fprintf(stderr,"Genesis: %s\n",e.what());return 1;}
}
