#pragma once
#include <string>
#include <string_view>
namespace genesis::asset_path {
// Archive names are byte strings (including legacy code-page bytes), not OS
// paths. Never round-trip them through Windows filesystem/locale conversion.
inline std::string parent(std::string_view path) {
    auto at = path.find_last_of("/\\");
    return at == std::string_view::npos ? std::string() : std::string(path.substr(0, at));
}
inline std::size_t extension_at(std::string_view path) {
    auto slash = path.find_last_of("/\\"), dot = path.find_last_of('.');
    auto begin = slash == std::string_view::npos ? 0 : slash + 1;
    return dot != std::string_view::npos && dot > begin ? dot : path.size();
}
inline std::string extension(std::string_view path) {
    std::string result(path.substr(extension_at(path)));
    for (auto& c : result)
        if (c >= 'A' && c <= 'Z')
            c += 32;
    return result;
}
inline std::string stem(std::string_view path) {
    auto slash = path.find_last_of("/\\");
    auto begin = slash == std::string_view::npos ? 0 : slash + 1;
    return std::string(path.substr(begin, extension_at(path) - begin));
}
inline std::string join(std::string_view root, std::string_view child) {
    std::string result(root);
    if (!result.empty() && result.back() != '/' && result.back() != '\\')
        result.push_back('/');
    result.append(child);
    return result;
}
inline std::string replace_extension(std::string_view path, std::string_view extension) {
    return std::string(path.substr(0, extension_at(path))) + std::string(extension);
}
} // namespace genesis::asset_path
