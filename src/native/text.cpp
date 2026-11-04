#include "text.h"
#include <algorithm>
#include <array>
#include <cmath>

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
namespace {
std::string encode_utf8(const std::vector<char32_t>& points, std::size_t start, std::size_t count) {
    std::string out;
    for (std::size_t i = start; i < start + count && i < points.size(); ++i) {
        char32_t c = points[i];
        if (c < 0x80)
            out += char(c);
        else if (c < 0x800) {
            out += char(0xc0 | c >> 6);
            out += char(0x80 | (c & 63));
        } else if (c < 0x10000) {
            out += char(0xe0 | c >> 12);
            out += char(0x80 | (c >> 6 & 63));
            out += char(0x80 | (c & 63));
        } else {
            out += char(0xf0 | c >> 18);
            out += char(0x80 | (c >> 12 & 63));
            out += char(0x80 | (c >> 6 & 63));
            out += char(0x80 | (c & 63));
        }
    }
    return out;
}
std::vector<std::string> split_lines(std::string_view utf8) {
    std::vector<std::string> lines(1);
    for (char c : utf8) {
        if (c == '\n')
            lines.emplace_back();
        else
            lines.back() += c;
    }
    for (auto& line : lines)
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
    return lines;
}
} // namespace
OutlineSegments parse_outline_segments(const std::vector<char32_t>& text) {
    const std::size_t count = text.size();
    OutlineSegments result;
    result.part[0] = {0, count};
    result.part[1] = result.part[2] = {count, 0};
    std::size_t marker = count;
    for (std::size_t i = 0; i + 1 < count; ++i)
        if (text[i] == U'<' && text[i + 1] == U'<') {
            marker = i;
            break;
        }
    if (marker < count && count - marker >= 8 && text[marker + 5] == U'c') {
        int requested = 0, sign = 1;
        std::size_t index = 0;
        while (index < 3 && text[marker + 2 + index] == U' ')
            ++index;
        if (index < 3 && (text[marker + 2 + index] == U'+' || text[marker + 2 + index] == U'-')) {
            sign = text[marker + 2 + index] == U'-' ? -1 : 1;
            ++index;
        }
        while (index < 3 && text[marker + 2 + index] >= U'0' && text[marker + 2 + index] <= U'9') {
            requested = requested * 10 + int(text[marker + 2 + index] - U'0');
            ++index;
        }
        requested *= sign;
        if (requested > 0) {
            const std::size_t start = marker + 8, end = std::min(count, start + std::size_t(requested));
            result.part[0] = {0, marker};
            result.part[1] = {start, end - std::min(end, start)};
            result.part[2] = {end, count - end};
        }
    }
    return result;
}
TextSize Text::measure_block(std::string_view utf8) {
    TextSize size{0, font_.line_height()};
    bool first = true;
    for (const auto& line : split_lines(utf8)) {
        size.width = std::max(size.width, font_.measure(line).width);
        if (!first)
            size.height += font_.line_height();
        first = false;
    }
    return size;
}
int Text::draw_in_rect(std::string_view utf8, TextRect& rect, unsigned flags, std::uint32_t argb) {
    const auto block = measure_block(utf8);
    const int height = int(block.height);
    if (flags & text_calcrect) {
        rect.right = rect.left + int(block.width);
        rect.bottom = rect.top + height;
        return height;
    }
    float y = float(rect.top);
    if (flags & text_vcenter)
        y += float(rect.bottom - rect.top - height) * .5f;
    else if (flags & text_bottom)
        y += float(rect.bottom - rect.top - height);
    GLint was_enabled = 0;
    std::array<GLint, 4> box = {};
    renderer_.api().GetIntegerv(GL_SCISSOR_TEST, &was_enabled);
    renderer_.api().GetIntegerv(GL_SCISSOR_BOX, box.data());
    if (!(flags & text_noclip))
        renderer_.scissor(rect.left, rect.top, std::max(0, rect.right - rect.left),
                          std::max(0, rect.bottom - rect.top));
    for (const auto& line : split_lines(utf8)) {
        if (!line.empty()) {
            const float advance = font_.measure(line).width;
            float x = float(rect.left);
            if (flags & text_center)
                x += float(rect.right - rect.left) * .5f - advance * .5f;
            else if (flags & text_right)
                x += float(rect.right - rect.left) - advance;
            draw(line, x, y, argb);
        }
        y += font_.line_height();
    }
    if (!(flags & text_noclip)) {
        if (was_enabled) {
            renderer_.api().Scissor(box[0], box[1], box[2], box[3]);
            renderer_.api().Enable(GL_SCISSOR_TEST);
        } else
            renderer_.reset_scissor();
    }
    return height;
}
OutlinedLabel Text::outlined_label(std::string_view utf8) {
    const auto points = decode_utf8(utf8);
    const auto segments = parse_outline_segments(points);
    std::string text[3];
    int widths[3];
    int total = 2;
    for (int i = 0; i < 3; ++i) {
        text[i] = encode_utf8(points, segments.part[i].start, segments.part[i].count);
        widths[i] = text[i].empty() ? 0 : int(std::ceil(font_.measure(text[i]).width));
        total += widths[i];
    }
    const int height = int(font_.line_height()) + 2;
    if (total > 16384 || height > 16384)
        throw std::invalid_argument("Outlined label is too large");
    const auto target = renderer_.create_target(total, height);
    std::vector<std::uint8_t> pixels;
    {
        TargetScope scope(renderer_, target);
        renderer_.clear({0, 0, 1, 1});
        static const int dx[5] = {1, 1, 0, 2, 1}, dy[5] = {0, 2, 1, 1, 1};
        int cursor = 0;
        for (int i = 0; i < 3; ++i) {
            if (text[i].empty())
                continue;
            for (int pass = 0; pass < 5; ++pass)
                draw(text[i], float(cursor + dx[pass]), float(dy[pass]),
                     pass == 4 ? 0xffffffffu : 0xff000000u);
            cursor += widths[i];
        }
        pixels = renderer_.read_pixels();
    }
    for (std::size_t i = 0; i < pixels.size(); i += 4)
        pixels[i + 3] = (pixels[i] < 128 && pixels[i + 2] >= 128) ? 0 : 255;
    // Keyed pixels also drop to transparent black like the original (value 0).
    for (std::size_t i = 0; i < pixels.size(); i += 4)
        if (!pixels[i + 3])
            pixels[i] = pixels[i + 1] = pixels[i + 2] = 0;
    return {renderer_.texture_rgba(total, height, pixels.data()), total, height};
}
} // namespace genesis
