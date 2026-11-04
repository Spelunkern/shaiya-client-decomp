#include "crypto/aes.h"
#include "crypto/sha256.h"
#include "crypto/lcg_stream.h"
#include "crypto/transport.h"

extern "C" {
uint8_t dat_02251fe0[4096] = {};
uint32_t dat_02252fe4 = 0;
uint8_t dat_02252fe8 = 0, dat_02252fe9 = 0, dat_02252fea = 0;
}
#include <cstring>
#include <cstdio>

int main() {
    const unsigned char sha_expected[32] = {0xba,0x78,0x16,0xbf,0x8f,0x01,0xcf,0xea,0x41,0x41,0x40,0xde,0x5d,0xae,0x22,0x23,0xb0,0x03,0x61,0xa3,0x96,0x17,0x7a,0x9c,0xb4,0x10,0xff,0x61,0xf2,0x00,0x15,0xad};
    unsigned char digest[32];
    sha256_digest(digest, "abc", 3);
    if (std::memcmp(digest, sha_expected, 32)) { std::puts("SHA-256 failed"); return 1; }
    const unsigned char key[16] = {0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f};
    const unsigned char plain[16] = {0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88,0x99,0xaa,0xbb,0xcc,0xdd,0xee,0xff};
    const unsigned char encrypted[16] = {0x69,0xc4,0xe0,0xd8,0x6a,0x7b,0x04,0x30,0xd8,0xcd,0xb7,0x80,0x70,0xb4,0xc5,0x5a};
    AesSchedule schedule{}; unsigned char block[16];
    if (aes_expand_key(&schedule, key, 128) || (aes_encrypt_block(&schedule, plain, block), std::memcmp(block, encrypted, 16))) { std::puts("AES failed"); return 2; }
    LcgStreamState a{}, b{};
    for (unsigned i=0; i<8; ++i) a.seeds[i] = b.seeds[i] = i + 1;
    a.position = b.position = 16;
    unsigned char input[37]{}, whole[37]{}, split[37]{};
    lcg_transform_stream(&a, input, whole, 37);
    lcg_transform_stream(&b, input, split, 7);
    lcg_transform_stream(&b, input + 7, split + 7, 30);
    if (std::memcmp(whole, split, 37)) { std::puts("LCG streaming failed"); return 3; }
    ClientCryptoState state{};
    uint8_t material[32] = {};
    initialize_transport_aes_material(&state, material);
    unsigned char message[37], original[37];
    for (unsigned i = 0; i < sizeof(message); ++i) message[i] = original[i] = static_cast<unsigned char>(i);
    transform_outbound_payload(&state, message, sizeof(message));
    if (!std::memcmp(message, original, sizeof(message))) { std::puts("Transport did not encrypt"); return 4; }
    transform_inbound_payload(&state, message, sizeof(message));
    if (std::memcmp(message, original, sizeof(message))) { std::puts("Transport round trip failed"); return 5; }
    return 0;
}
