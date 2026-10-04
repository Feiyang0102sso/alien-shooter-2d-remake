/** @file renderer.cpp
 * @brief Load textures and draw sprites, parts, tiles, and bitmap text.
 */
#include "render/renderer.h"
#include <iostream>
#include <stdexcept>

namespace alien {
namespace {
// Lowercase letters share the uppercase glyphs; unknown bytes draw glyph 0 (space).
int glyphId(unsigned char character, int glyphCount) {
    if (character >= 'a' && character <= 'z') { character = static_cast<unsigned char>(character - 'a' + 'A'); }
    if (character < layout::FontFirstCharacter || character >= layout::FontFirstCharacter + glyphCount) { return 0; }
    return character - layout::FontFirstCharacter;
}
}
Renderer::Renderer(const Assets& assets) : assets_(assets) {
    for (const std::string& name : assets.textures) {
        const auto path = (assets.root() / name).u8string();
        auto texture = LoadTexture(path.c_str());
        if (!IsTextureValid(texture)) { throw std::runtime_error("Cannot load texture: " + path); }
        SetTextureFilter(texture, TEXTURE_FILTER_POINT);
        textures_.emplace(name, texture);
    }
    std::cout << "Renderer loaded " << textures_.size() << " textures.\n";
}
Renderer::~Renderer() { for (auto& entry : textures_) { UnloadTexture(entry.second); } }

void Renderer::sprite(const Sprite& image, int x, int y, bool offset, Color tint) const {
    if (image.texture.empty() || image.width == 0 || image.height == 0) { return; }
    if (offset) { x += image.offsetX; y += image.offsetY; }
    Rectangle source{static_cast<float>(image.x), static_cast<float>(image.y), static_cast<float>(image.width), static_cast<float>(image.height)};
    DrawTextureRec(textures_.at(image.texture), source, Vector2{static_cast<float>(x), static_cast<float>(y)}, tint);
}
void Renderer::image(const std::string& name, int x, int y) const { sprite(assets_.interfaceImages.at(name), x, y); }
void Renderer::part(int id, int x, int y, int direction) const {
    if (id <= 0 || id >= static_cast<int>(assets_.parts.size())) { return; }
    int side = 0;
    if (direction < 0) { side = 1; }
    sprite(assets_.parts[id][side], x, y);
}
void Renderer::tile(int theme, int id, int x, int y, bool offset) const {
    const auto& themeTiles = assets_.tiles[theme];
    if (id <= 0 || id >= static_cast<int>(themeTiles.size())) { return; }
    const auto& tile = themeTiles[id];
    if (!tile.image.texture.empty()) { sprite(tile.image, x, y, offset); }
    else if (tile.base > 0 && tile.overlay > 0) {
        sprite(themeTiles[tile.base].image, x, y, offset);
        sprite(themeTiles[tile.overlay].image, x, y, true);
    }
}
int Renderer::textWidth(const std::string& value) const {
    const int glyphCount = static_cast<int>(assets_.glyphs.size());
    int width = 0;
    for (unsigned char character : value) { width += assets_.glyphs[glyphId(character, glyphCount)].advance; }
    if (!value.empty()) { --width; }
    return width;
}
void Renderer::text(const std::string& value, int x, int y, int alignment, Color tint, int spacing) const {
    const int glyphCount = static_cast<int>(assets_.glyphs.size());
    int width = textWidth(value);
    if (!value.empty()) { width += spacing * (static_cast<int>(value.size()) - 1); }
    if (alignment == 1) { x -= width / 2; }
    if (alignment == 2) { x -= width; }
    for (unsigned char character : value) {
        const auto& glyph = assets_.glyphs[glyphId(character, glyphCount)];
        sprite(glyph.image, x, y + layout::GlyphHeight - glyph.image.height, true, tint);
        x += glyph.advance + spacing;
    }
}
}
