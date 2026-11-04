#pragma once
#include "loopback.h"
#include "crypto/sha256.h"

namespace fixture {
// Isolated protocol peer for application input tests. All account data is
// synthetic; no database or external service is contacted.
class GameServers {
public:
    Endpoint login,world;
    bool reject_first=true,selected=false,entered=false;
    unsigned attempts=0;
    std::array<std::uint8_t,8> equipment_types{},equipment_ids{};
    std::string expected_user="Jos\xe9",expected_password="retry";
    GameServers() {
        modulus_.fill(255);std::array<std::uint8_t,64> exponent={1};
        Writer challenge;challenge.u8(0);challenge.u8(1);challenge.u8(128);
        challenge.append(exponent.data(),64);challenge.append(modulus_.data(),128);
        login.send({0xa101,challenge.finish()});
    }
    void pump() {
        login.pump();world.pump();
        while(auto packet=login.wire.pop()) {
            if(packet->opcode==0xa101) {
                if(packet->body.size()!=129) throw std::runtime_error("Fixture RSA reply framing");
                hmac_sha256(material_.data(),packet->body.data()+1,128,modulus_.data(),128);login.wire.crypto.aes_material(material_);
            } else if(packet->opcode==0xa102) {
                Reader r(packet->body);auto username=r.text(32),password=r.text(16);
                if(username!=expected_user) throw std::runtime_error("Application did not encode the account as CP1252");
                ++attempts;
                if(reject_first && attempts==1) {login.send({0xa102,{1}});continue;}
                if(password!=expected_password) throw std::runtime_error("Application submitted an unexpected synthetic password");
                Writer result;result.u8(0);result.u32(42);result.u8(11);identity_.fill(0x42);result.append(identity_.data(),16);
                login.send({0xa102,result.finish()});Writer list;list.u8(1);list.u8(1);list.u8(0);list.u16(7);list.u16(100);list.text("G\xe9nesis",32);
                login.send({0xa201,list.finish()});
            } else if(packet->opcode==0xa202) {
                if(packet->body!=Bytes({1,0,0x22,0x12,0x11})) throw std::runtime_error("Application server-selection request");
                selected=true;login.send({0xa202,{0,127,0,0,1}});
            } else throw std::runtime_error("Unexpected application/login opcode");
        }
        while(auto packet=world.wire.pop()) {
            if(packet->opcode==0xa301) {
                Reader r(packet->body);if(r.u32()!=42 || r.fixed<16>()!=identity_) throw std::runtime_error("Application world identity");
                std::array<std::uint8_t,32> digest;sha256_digest(digest.data(),material_.data()+16,16);std::copy_n(digest.data(),16,material_.begin()+16);
                world.wire.crypto.aes_material(material_);Writer result;result.u8(0);result.u8(3);for(int i=0;i<16;++i) result.u8(0);
                world.send({0xa301,result.finish()});
                for(unsigned slot=0;slot<5;++slot) {
                    Writer data;data.u8(slot);data.u32(slot==2?1234:0);
                    if(slot==2) {
                        data.u32(0);data.u16(42);for(int i=0;i<7;++i) data.u8(1);data.u16(0);
                        for(int i=0;i<9;++i) data.u16(100+i);
                        data.append(equipment_types.data(),equipment_types.size());
                        data.append(equipment_ids.data(),equipment_ids.size());
                        data.text("Espa\xf1" "a",19);data.u8(0);data.u8(0);
                    }
                    world.send({0x0101,data.finish()});
                }
                world.send({0x0109,{1,3}});
            } else if(packet->opcode==0x0104) {
                Reader r(packet->body);if(r.u32()!=1234) throw std::runtime_error("Application selected the wrong character");
                Writer result;result.u8(0);result.u32(1234);world.send({0x0104,result.finish()});entered=true;
            } else throw std::runtime_error("Unexpected application/world opcode");
        }
    }
private:
    std::array<std::uint8_t,128> modulus_;
    std::array<std::uint8_t,32> material_={};
    std::array<std::uint8_t,16> identity_={};
};
}
