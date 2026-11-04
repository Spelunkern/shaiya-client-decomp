#include "text.h"

namespace genesis {
namespace {
constexpr int atlas_size = 1024;
}
Text::~Text() {
    for (const auto& page : pages_)
        renderer_.release_texture(page.texture);
}
Text::Region Text::region(char32_t point, const Glyph& glyph) {
    auto cached = regions_.find(point);
    if (cached != regions_.end())
        return cached->second;
    if (glyph.width + 2 > atlas_size || glyph.height + 2 > atlas_size)
        throw std::runtime_error("Glyph is too large for atlas");
    if (!pages_.empty()) {
        auto& page = pages_.back();
        if (page.x + glyph.width + 1 > atlas_size) {
            page.x = 1;
            page.y += page.row_height + 1;
            page.row_height = 0;
        }
    }
    if (pages_.empty() || pages_.back().y + glyph.height + 1 > atlas_size) {
        if (pages_.size() >= 16)
            throw std::runtime_error("Glyph atlas page limit exceeded");
        std::vector<std::uint8_t> rgba(atlas_size * atlas_size * 4, 255);
        for (std::size_t i = 3; i < rgba.size(); i += 4)
            rgba[i] = 0;
        pages_.push_back({renderer_.texture_rgba(atlas_size, atlas_size, rgba.data()), 1, 1, 0});
    }
    auto& page = pages_.back();
    Region result{page.texture, page.x, page.y};
    std::vector<std::uint8_t> rgba(std::size_t(glyph.width) * glyph.height * 4, 255);
    for (std::size_t i = 0; i < glyph.coverage.size(); ++i)
        rgba[i * 4 + 3] = glyph.coverage[i];
    renderer_.update_texture(page.texture, page.x, page.y, glyph.width, glyph.height, rgba.data());
    page.x += glyph.width + 1;
    page.row_height = std::max(page.row_height, glyph.height);
    regions_.emplace(point, result);
    return result;
}
void Text::draw(std::string_view utf8, float x, float y, std::uint32_t argb,float scale) {
    if(!(scale>0)) throw std::invalid_argument("Invalid text scale");
    float cursor = x, baseline = y + font_.ascender()*scale;
    unsigned previous = 0;
    std::map<GLuint, std::vector<UiVertex>> batches;
    for (auto point : decode_utf8(utf8)) {
        if (point == '\n') {
            cursor = x;
            baseline += font_.line_height()*scale;
            previous = 0;
            continue;
        }
        const auto& glyph = font_.glyph(point);
        cursor += font_.kerning(previous, glyph.index)*scale;
        if (glyph.width && glyph.height) {
            auto atlas = region(point, glyph);
            auto& vertices = batches[atlas.texture];
            float left = cursor + glyph.left*scale, top = baseline - glyph.top*scale;
            float right = left + glyph.width*scale, bottom = top + glyph.height*scale;
            float u = float(atlas.x) / atlas_size, v = float(atlas.y) / atlas_size;
            float ur = float(atlas.x + glyph.width) / atlas_size,
                  vb = float(atlas.y + glyph.height) / atlas_size;
            const UiVertex quad[] = {
                {left, top, 0, 1, argb, u, v},       {right, top, 0, 1, argb, ur, v},
                {right, bottom, 0, 1, argb, ur, vb}, {left, top, 0, 1, argb, u, v},
                {right, bottom, 0, 1, argb, ur, vb}, {left, bottom, 0, 1, argb, u, vb}};
            vertices.insert(vertices.end(), std::begin(quad), std::end(quad));
        }
        cursor += glyph.advance*scale;
        previous = glyph.index;
    }
    ObjectUniforms object;
    object.parameters[0] = 0;
    renderer_.object(object);
    for (const auto& batch : batches) {
        renderer_.bind_images({batch.first, 0, 0, 0});
        renderer_.draw(Layout::ui, VertexPass::ui, PixelPass::ui, batch.second.data(),
                       batch.second.size(),
                       {Blend::alpha, Depth::disabled, Cull::none, Sampling::linear_clamp});
    }
}
} // namespace genesis
