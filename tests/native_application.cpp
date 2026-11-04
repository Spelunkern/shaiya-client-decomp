#include "application.h"
#include "item.h"
#include "sdata.h"
#include "support/game_servers.h"
#include <chrono>
#include <cstdio>
#include <fstream>

using namespace genesis;
namespace {
// Drive the same SDL_Event handler as the production event loop. sdl2-compat
// does not support synthesizing SDL_TEXTINPUT through SDL_PushEvent.
void expect(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
void key(Application& app,SDL_Keycode code,SDL_Keymod modifiers=KMOD_NONE) {
    SDL_Event event={};event.type=SDL_KEYDOWN;event.key.windowID=SDL_GetWindowID(app.renderer().window());event.key.keysym.sym=code;event.key.keysym.mod=modifiers;
    app.handle_event(event);
}
void text(Application& app,const char* value) {
    SDL_Event event={};event.type=SDL_TEXTINPUT;event.text.windowID=SDL_GetWindowID(app.renderer().window());SDL_strlcpy(event.text.text,value,sizeof(event.text.text));
    app.handle_event(event);
}
void click(Application& app,int x,int y) {
    SDL_Event event={};event.type=SDL_MOUSEBUTTONDOWN;event.button.windowID=SDL_GetWindowID(app.renderer().window());event.button.button=SDL_BUTTON_LEFT;event.button.x=x;event.button.y=y;
    app.handle_event(event);
}
}
int main(int argc,char** argv) {
    if(argc<2) return 2;
    const auto temp=std::filesystem::temp_directory_path()/("genesis-app-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        fixture::GameServers server;AppSettings settings;settings.assets=argv[1];settings.hidden=true;
        auto archive=shaiya::portable::Archive::open(settings.assets/"data.sah",settings.assets/"data.saf");
        auto items=decode_items(decode_sdata(archive.read("data/Item/Item.SData")));
        bool found=false;
        for(unsigned type=1;type<=items.groups.size() && !found;++type)
            for(unsigned id=1;id<=items.groups[type-1].size();++id)
                if(items.find(std::uint8_t(type),std::uint8_t(id))->model()==1) {
                    for(unsigned slot=0;slot<5;++slot) {
                        server.equipment_types[slot]=std::uint8_t(type);
                        server.equipment_ids[slot]=std::uint8_t(id);
                    }
                    found=true;break;
                }
        expect(found,"Missing original item for equipped application fixture");
        // Weapon and shield slots must not suppress the character preview,
        // even while their separate attachment rendering is unfinished.
        server.equipment_types[5]=server.equipment_types[0];
        server.equipment_ids[5]=server.equipment_ids[0];
        server.equipment_types[6]=server.equipment_types[0];
        server.equipment_ids[6]=server.equipment_ids[0];
        settings.login_port=server.login.port();settings.world_port=server.world.port();settings.preferences=temp/"id.txt";
        {
            auto glowing=settings;glowing.glow_quality=2;
            Application lit(glowing);expect(lit.frame(),"Frame renders with glow enabled");
            expect(lit.renderer().resource_counts().targets==3,"Glow allocates its three targets once");
            lit.frame();expect(lit.renderer().resource_counts().targets==3,"Glow targets are reused across frames");
        }
        {
            Application plain(settings);expect(plain.frame(),"Frame renders without glow");
            expect(plain.renderer().resource_counts().targets==0,"No glow targets when disabled");
        }
        {
            Application app(settings);expect(app.frame() && app.screen()==Application::Screen::login,"Native entry screen");
            text(app,"José");key(app,SDLK_TAB);text(app,"secret");app.frame();auto masked=app.renderer().read_pixels();
            key(app,SDLK_a,KMOD_CTRL);text(app,"xxxxxx");app.frame();
            expect(masked==app.renderer().read_pixels(),"Password characters were rendered instead of a mask");
            key(app,SDLK_a,KMOD_CTRL);text(app,"synthetic");click(app,553,632);app.frame();
            if(argc>=3) app.capture(std::filesystem::path(argv[2]).string()+"-login.ppm");
            key(app,SDLK_RETURN);app.frame();
            auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(12);
            while(std::chrono::steady_clock::now()<deadline && (server.attempts<1 || app.screen()!=Application::Screen::login)) {server.pump();app.frame();SDL_Delay(1);}
            expect(server.attempts==1 && app.screen()==Application::Screen::login && !app.can_confirm(),"Rejected login did not clear the password and restore input");
            text(app,"retry");key(app,SDLK_RETURN);app.frame();
            while(std::chrono::steady_clock::now()<deadline && !(app.screen()==Application::Screen::servers && app.can_confirm())) {server.pump();app.frame();SDL_Delay(1);}
            expect(server.attempts==2 && app.screen()==Application::Screen::servers && app.can_confirm(),"Interactive retry did not reach server selection");
            if(argc>=3) app.capture(std::filesystem::path(argv[2]).string()+"-servers.ppm");
            click(app,480,247);click(app,510,566);app.frame();
            while(std::chrono::steady_clock::now()<deadline && !(app.screen()==Application::Screen::characters && app.can_confirm())) {server.pump();app.frame();SDL_Delay(1);}
            expect(server.selected && app.screen()==Application::Screen::characters && app.can_confirm(),"Server UI did not enter the world character list");
            expect(app.character_preview_ready(),"Selected equipped character was not integrated into the native UI");
            if(argc>=3) app.capture(std::filesystem::path(argv[2]).string()+"-characters.ppm");
            click(app,110,195);click(app,510,696);app.frame();
            while(std::chrono::steady_clock::now()<deadline && !(server.entered && app.screen()==Application::Screen::loading)) {server.pump();app.frame();SDL_Delay(1);}
            expect(server.entered && app.screen()==Application::Screen::loading,"Character UI did not submit its ID and begin loading");
            expect(app.terrain_ready(),"Selected FLD terrain did not load during character transition");
            expect(app.terrain_material_count()==6,"Selected FLD material records were not retained");
            expect(app.terrain_surface_sound(0,0) && *app.terrain_surface_sound(0,0)=="ch_walke001.wav", "Selected FLD surface sound lookup failed");
            expect(app.water_ready(),"Field water definition did not load");
            expect(app.objects_ready(),"Field model placements did not load");
            expect(app.character_preview_ready(),"World actor was not kept for the loading scene");
            app.frame();expect(app.world_objects_drawn()>0,"No world model was drawn near the player");
            expect(app.world_actor_shadow_ready(),"World actor shadow was not rendered onto the terrain");
            if(argc>=3) app.capture(std::filesystem::path(argv[2]).string()+"-loading.ppm");
            std::ifstream preference(settings.preferences,std::ios::binary);std::string contents((std::istreambuf_iterator<char>(preference)),{});
            expect(contents=="José\n","Save ID did not persist only the account name");
            key(app,SDLK_ESCAPE);expect(!app.frame(),"Escape did not close native application");
        }
        // An indoor DUN descriptor selects its DG instead of a terrain field.
        {
            fixture::GameServers indoor;indoor.reject_first=false;indoor.expected_password="secret";indoor.map=106;
            indoor.equipment_types=server.equipment_types;indoor.equipment_ids=server.equipment_ids;
            auto dungeon_settings=settings;dungeon_settings.login_port=indoor.login.port();dungeon_settings.world_port=indoor.world.port();
            dungeon_settings.preferences=temp/"dungeon-id.txt";
            Application app(dungeon_settings);app.frame();
            text(app,"José");key(app,SDLK_TAB);text(app,"secret");key(app,SDLK_RETURN);app.frame();
            auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(12);
            while(std::chrono::steady_clock::now()<deadline && !(app.screen()==Application::Screen::servers && app.can_confirm())) {indoor.pump();app.frame();SDL_Delay(1);}
            click(app,480,247);click(app,510,566);app.frame();
            while(std::chrono::steady_clock::now()<deadline && !(app.screen()==Application::Screen::characters && app.can_confirm())) {indoor.pump();app.frame();SDL_Delay(1);}
            click(app,110,195);click(app,510,696);app.frame();
            while(std::chrono::steady_clock::now()<deadline && !(indoor.entered && app.screen()==Application::Screen::loading)) {indoor.pump();app.frame();SDL_Delay(1);}
            expect(indoor.entered && app.screen()==Application::Screen::loading,"Dungeon fixture did not reach loading");
            app.frame();
            expect(app.dungeon_ready() && !app.terrain_ready(),"Selected DUN descriptor did not load its dungeon");
            if(argc>=3) app.capture(std::filesystem::path(argv[2]).string()+"-dungeon.ppm");
        }
        // Saved ID is restored through the normal constructor and UI input.
        {
            Application restored(settings);restored.frame();expect(!restored.can_confirm(),"Restored a password with Save ID");
            auto pixels=restored.renderer().read_pixels();unsigned account_ink=0;
            if(argc>=3) restored.capture(std::filesystem::path(argv[2]).string()+"-restored.ppm");
            key(restored,SDLK_a,KMOD_CTRL);key(restored,SDLK_BACKSPACE);restored.frame();
            auto cleared=restored.renderer().read_pixels();
            for(int y=579;y<596;++y) for(int x=481;x<605;++x) {
                auto at=(std::size_t(y)*restored.renderer().width()+x)*4;
                if(pixels[at]!=cleared[at] || pixels[at+1]!=cleared[at+1] || pixels[at+2]!=cleared[at+2]) ++account_ink;
            }
            expect(account_ink>20,"Saved account was not restored into the rendered input field");
            click(restored,553,632);restored.frame();
        }
        expect(!std::filesystem::exists(settings.preferences),"Unchecking Save ID did not remove the saved account");
        std::filesystem::remove_all(temp);std::puts("Native SDL/OpenGL login/server/character UI integration passed");return 0;
    }catch(const std::exception& e) {std::filesystem::remove_all(temp);std::fprintf(stderr,"%s\n",e.what());return 1;}
}
