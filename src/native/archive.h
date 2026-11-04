#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace shaiya::portable {
struct ArchiveFile { std::uint64_t offset; std::uint32_t size; std::string name; };
struct ArchiveDirectory {
    std::string name;
    std::unordered_map<std::uint32_t, ArchiveFile> files;
    std::unordered_map<std::uint32_t, ArchiveDirectory> directories;
};
std::uint32_t archive_name_hash(std::string_view name);
class Archive {
public:
    static Archive open(const std::filesystem::path& index, const std::filesystem::path& data);
    const ArchiveFile* find(std::string_view path) const;
    std::vector<std::uint8_t> read(std::string_view path) const;
    std::vector<std::string> paths() const;
private:
    ArchiveDirectory root_;
    std::filesystem::path data_;
};
}
