#include "archive.h"
#include "crypto/sha256.h"
#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
#include <limits>

int main(int argc, char** argv) {
    if (argc != 3 && argc != 4) {
        std::fprintf(stderr, "Usage: shaiya_archive <asset-directory> <data/path|--list> [output-file]\n");
        return 2;
    }
    try {
        const std::filesystem::path base(argv[1]);
        auto archive = shaiya::portable::Archive::open(base / "data.sah", base / "data.saf");
        if (std::string(argv[2]) == "--list") {
            for (const auto& path : archive.paths()) std::puts(path.c_str());
            return 0;
        }
        const auto* entry = archive.find(argv[2]);
        if (!entry) throw std::runtime_error("Resource not found");
        auto bytes = archive.read(argv[2]);
        Sha256State state{};
        sha256_initialize(&state);
        std::size_t offset = 0;
        while (offset < bytes.size()) {
            auto remaining = bytes.size() - offset;
            auto chunk = static_cast<std::uint32_t>(remaining > 1048576 ? 1048576 : remaining);
            sha256_update(&state, bytes.data() + offset, chunk);
            offset += chunk;
        }
        std::uint8_t digest[32]; sha256_finalize(&state, digest);
        std::printf("offset=%llu size=%zu sha256=", static_cast<unsigned long long>(entry->offset), bytes.size());
        for (auto byte : digest) std::printf("%02x", byte);
        std::puts("");
        if (argc == 4) {
            const std::filesystem::path output(argv[3]);
            if (std::filesystem::exists(output)) throw std::runtime_error("Output already exists");
            std::ofstream stream(output, std::ios::binary);
            if (!stream || !stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())))
                throw std::runtime_error("Cannot write extracted resource");
            stream.close();
            if (!stream) throw std::runtime_error("Cannot finish extracted resource");
        }
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what()); return 1;
    }
}
