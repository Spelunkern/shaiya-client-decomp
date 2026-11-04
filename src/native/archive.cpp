#include "archive.h"
#include <fstream>
#include <limits>
#include <stdexcept>
#include <utility>
#include <algorithm>
#include <functional>

namespace shaiya::portable {
std::uint32_t archive_name_hash(std::string_view name) {
    std::uint32_t hash = 0x1505;
    for (unsigned char c : name) {
        int value = c < 128 ? c : int(c) - 256;
        if (value >= 'A' && value <= 'Z') value += 'a' - 'A';
        hash = hash * 33u + static_cast<std::uint32_t>(value);
    }
    return hash;
}
namespace {
struct Reader {
    std::ifstream in;
    bool wide = false;
    std::size_t entries = 0;
    explicit Reader(const std::filesystem::path& path) : in(path, std::ios::binary) {
        if (!in) throw std::runtime_error("Cannot open SAH index");
    }
    void bytes(void* dst, std::size_t size) {
        if (!in.read(static_cast<char*>(dst), static_cast<std::streamsize>(size)))
            throw std::runtime_error("Truncated SAH index");
    }
    std::uint32_t u32() {
        unsigned char b[4]; bytes(b, 4);
        return std::uint32_t(b[0]) | std::uint32_t(b[1]) << 8 | std::uint32_t(b[2]) << 16 | std::uint32_t(b[3]) << 24;
    }
    std::string name() {
        auto n = u32();
        if (!n || n > 1024) throw std::runtime_error("Invalid SAH name length");
        std::string value(n, '\0'); bytes(value.data(), n);
        if (value.back() != '\0' || value.find('\0') != n - 1) throw std::runtime_error("Invalid SAH name");
        value.pop_back(); return value;
    }
    std::uint32_t count() {
        auto n = u32();
        if (n > 1000000 || entries + n > 1000000) throw std::runtime_error("Invalid SAH entry count");
        entries += n; return n;
    }
    std::pair<std::string, ArchiveDirectory> directory(unsigned depth) {
        if (depth > 256) throw std::runtime_error("SAH directory nesting limit exceeded");
        auto name_value = name(); ArchiveDirectory dir; dir.name = name_value;
        auto files = count();
        for (std::uint32_t i = 0; i < files; ++i) {
            auto filename = name(); std::uint64_t offset = u32();
            if (wide) offset |= std::uint64_t(u32()) << 32;
            auto size = u32(); (void)u32();
            auto hash = archive_name_hash(filename);
            dir.files[hash] = {offset, size, std::move(filename)};
        }
        auto children = count();
        for (std::uint32_t i = 0; i < children; ++i) {
            auto child = directory(depth + 1);
            dir.directories[archive_name_hash(child.first)] = std::move(child.second);
        }
        return {std::move(name_value), std::move(dir)};
    }
};
}
Archive Archive::open(const std::filesystem::path& index, const std::filesystem::path& data) {
    Reader r(index); char signature[3]; r.bytes(signature, 3);
    r.wide = signature[0] == 'S' && signature[1] == 'A' && signature[2] == 'H';
    if (r.wide) { unsigned char header[48]; r.bytes(header, sizeof(header)); }
    else { r.in.seekg(0); unsigned char header[8]; r.bytes(header, sizeof(header)); }
    Archive result; result.root_ = std::move(r.directory(0).second); result.data_ = data;
    return result;
}
const ArchiveFile* Archive::find(std::string_view path) const {
    if (path.size() < 5 || (path[4] != '/' && path[4] != '\\')) return nullptr;
    constexpr char prefix[] = "data";
    for (unsigned i = 0; i < 4; ++i) {
        unsigned char c = static_cast<unsigned char>(path[i]);
        if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
        if (c != prefix[i]) return nullptr;
    }
    path.remove_prefix(5); auto* directory = &root_;
    for (;;) {
        auto split = path.find_first_of("/\\");
        auto component = path.substr(0, split);
        if (split == std::string_view::npos) {
            auto it = directory->files.find(archive_name_hash(component));
            return it == directory->files.end() ? nullptr : &it->second;
        }
        auto it = directory->directories.find(archive_name_hash(component));
        if (it == directory->directories.end()) return nullptr;
        directory = &it->second; path.remove_prefix(split + 1);
    }
}
std::vector<std::uint8_t> Archive::read(std::string_view path) const {
    const auto* entry = find(path);
    if (!entry) throw std::runtime_error("Archive entry not found");
    std::ifstream in(data_, std::ios::binary);
    if (!in || entry->offset > std::uint64_t(std::numeric_limits<std::streamoff>::max())) throw std::runtime_error("Cannot open or seek SAF");
    in.seekg(0, std::ios::end);
    auto length = in.tellg();
    if (length < 0 || entry->offset > static_cast<std::uint64_t>(length) ||
        entry->size > static_cast<std::uint64_t>(length) - entry->offset)
        throw std::runtime_error("SAF entry exceeds file bounds");
    in.seekg(static_cast<std::streamoff>(entry->offset));
    std::vector<std::uint8_t> result(entry->size);
    if (!in || (entry->size && !in.read(reinterpret_cast<char*>(result.data()), entry->size))) throw std::runtime_error("Truncated SAF entry");
    return result;
}
std::vector<std::string> Archive::paths() const {
    std::vector<std::string> result;
    std::function<void(const ArchiveDirectory&, const std::string&)> visit;
    visit = [&](const ArchiveDirectory& dir, const std::string& parent) {
        for (const auto& entry : dir.files) result.push_back(parent + entry.second.name);
        for (const auto& entry : dir.directories)
            visit(entry.second, parent + entry.second.name + "/");
    };
    visit(root_, "data/");
    std::sort(result.begin(), result.end());
    return result;
}
}
