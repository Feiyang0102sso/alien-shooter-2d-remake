/** @file ui_renderer.cpp
 * @brief Draw menus, the sound prompt, message boxes, and rich text pages.
 */
#include "render/renderer.h"
#include "render/pixel.h"
#include <algorithm>

namespace alien {
namespace {
// Stack of short lines above a downward triangle, shown at the bottom right of overlays.
void continueArrow(int stemTop, int direction) {
    using namespace layout;
    for (int row = 0; row < ContinueStemRows; ++row) {
        const int lineY = stemTop + direction * row * 2;
        pixelLine(ContinueStemLeft, lineY, ContinueStemRight, lineY, PixelGreen);
    }
    pixelTriangle(ContinueArrowX, ContinueArrowY, ContinueArrowWidth, 1, PixelGreen);
}
}
MenuLayout menuLayout(int count, bool hasTitle, bool overlay) {
    MenuLayout result;
    if (overlay) {
        result.left = layout::PauseMenuLeft; result.top = layout::PauseMenuTop;
        result.width = layout::PauseMenuWidth; result.height = layout::PauseMenuHeight;
    }
    int titleHeight = 0;
    if (hasTitle) { titleHeight = layout::MenuTitleHeight; }
    const int available = result.height - titleHeight;
    const int used = 2 + count * layout::MenuItemHeight;
    result.firstItem = result.top + titleHeight + (available - used) / 2 + 2;
    return result;
}

void Renderer::soundPrompt(bool visible) const {
    using namespace layout;
    ClearBackground(BLACK);
    image("menu_background", 0, 0);
    if (visible) {
        text(assets_.text.label("sound_prompt_enable"), SoundPromptCenterX, SoundPromptFirstY, 1);
        text(assets_.text.label("sound_prompt_question"), SoundPromptCenterX, SoundPromptSecondY, 1);
    }
    DrawRectangle(0, SoftKeyBarY, ScreenWidth, SoftKeyBarHeight, BLACK);
    text(assets_.text.label("yes"), 2, SoftKeyTextY); text(assets_.text.label("no"), ScreenWidth - 2, SoftKeyTextY, 2);
}

void Renderer::menu(const std::string& title, const std::vector<std::string>& items, int selected,
                    const std::vector<bool>& enabled, bool overlay, int phase, int spread) const {
    using namespace layout;
    const auto box = menuLayout(static_cast<int>(items.size()), !title.empty(), overlay);
    const int x = box.left, y = box.top, width = box.width, height = box.height;
    const Color scanline = rgb(color::MenuScanline);
    if (!overlay) { ClearBackground(BLACK); image("menu_background", 0, 0); image("menu_overlay", MenuOverlayX, MenuOverlayY); }
    else { DrawRectangle(x, y, width, height, BLACK); }
    if (!overlay) {
        for (int line = y + 2; line < y + height; line += 2) { pixelLine(x + 2, line, x + width - 1, line, scanline); }
        pixelBorder(x, y, width, height, PixelGreen);
    } else { pixelBorder(x, y, width + 1, height + 1, PixelGreen); }
    pixelTriangle(x + width / 2 - 2, y - 2, MenuMarkerWidth, -1, BLACK);
    pixelTriangle(x + width / 2 - 2, y + height + 2, MenuMarkerWidth, 1, BLACK);
    if (!title.empty()) {
        DrawRectangle(x + 1, y + 1, width - 2, MenuTitleHeight, BLACK);
        text(title, x + width / 2, y + 4, 1);
        pixelLine(x, y + MenuTitleHeight, x + width - 1, y + MenuTitleHeight, PixelGreen);
    }
    for (int index = 0; index < static_cast<int>(items.size()); ++index) {
        const int itemY = box.firstItem + index * MenuItemHeight;
        if (index == selected) {
            // The selection is framed by two moving green gradient bands above and below the item.
            DrawRectangle(x + 2, itemY, width - 3, MenuTextHeight, BLACK);
            for (int column = 1; column < width - 1; ++column) {
                const int topShifted = (column + phase) % width;
                const int topIntensity = std::min(topShifted, width - 1 - topShifted) * 255 / (width / 2);
                const int shifted = (column + phase + 2) % width;
                const int bottomIntensity = std::min(shifted, width - 1 - shifted) * 255 / (width / 2);
                const Color topColor{0, static_cast<unsigned char>(topIntensity), 0, 255};
                const Color bottomColor{0, static_cast<unsigned char>(bottomIntensity), 0, 255};
                DrawPixel(x + column, itemY - 2, topColor); DrawPixel(x + column, itemY - 1, topColor);
                DrawPixel(x + column, itemY + MenuTextHeight, bottomColor); DrawPixel(x + column, itemY + MenuTextHeight + 1, bottomColor);
            }
        }
        if (index == selected && spread > 0) {
            // A newly selected item slides together from both halves.
            const auto split = items[index].size() / 2 + 1;
            BeginScissorMode(x + 2, itemY + 2, width - 4, MenuSpreadClipHeight);
            text(items[index].substr(0, split), x + 2 + width / 2 - spread, itemY + 2, 2, WHITE, spread);
            text(items[index].substr(split), x + 2 + width / 2 + spread, itemY + 2, 0, WHITE, spread);
            EndScissorMode();
            continue;
        }
        text(items[index], x + 2 + width / 2, itemY + 2, 1);
        if (index < static_cast<int>(enabled.size()) && !enabled[index]) {
            int firstLine = itemY + 1;
            if (((y & 1) != 0) != (((itemY + 2) & 1) == 0)) { --firstLine; }
            for (int line = firstLine; line <= itemY + MenuTextHeight; line += 2) { pixelLine(x + 1, line, x + width - 2, line, scanline); }
        }
    }
    const std::string& selectLabel = assets_.text.label("select");
    const std::string& backLabel = assets_.text.label("back");
    // The original sizes the left soft-key box by the BACK label.
    DrawRectangle(0, SoftKeyBarY, textWidth(backLabel) + 4, SoftKeyBarHeight, BLACK);
    pixelBorder(0, SoftKeyBarY, textWidth(backLabel) + 5, SoftKeyBarHeight, PixelGreen);
    text(selectLabel, 2, SoftKeyTextY);
    if (!overlay) {
        const int rightWidth = textWidth(selectLabel) + 4;
        DrawRectangle(ScreenWidth - rightWidth, SoftKeyBarY, rightWidth, SoftKeyBarHeight, BLACK);
        pixelBorder(ScreenWidth - rightWidth - 1, SoftKeyBarY, rightWidth + 1, SoftKeyBarHeight, PixelGreen);
        text(backLabel, ScreenWidth - 2, SoftKeyTextY, 2);
    } else {
        continueArrow(ContinueStemBottom, -1);
    }
}
float messageDuration(const std::vector<std::string>& lines) {
    int characters = 0;
    for (const auto& line : lines) { characters += static_cast<int>(line.size()); }
    return flow::MessageOpenSeconds + characters * flow::MessageCharacterSeconds;
}
void Renderer::message(const std::vector<std::string>& lines, bool failure, float age) const {
    using namespace layout;
    Color border = PixelGreen;
    if (failure) { border = rgb(color::Failure); }
    if (age < flow::MessageOpenSeconds) {
        const int step = std::min(MessageGrowSteps, static_cast<int>(age / flow::MessageCharacterSeconds));
        const int halfWidth = MessageGrowWidth + step * MessageGrowWidth, halfHeight = MessageGrowHeight + step * MessageGrowHeight;
        DrawRectangle(MessageCenterX - halfWidth, MessageCenterY - halfHeight, halfWidth * 2, halfHeight * 2, BLACK);
        pixelBorder(MessageCenterX - halfWidth, MessageCenterY - halfHeight, halfWidth * 2, halfHeight * 2 + 1, border);
        return;
    }
    int width = MessageMinWidth;
    for (const auto& line : lines) { width = std::max(width, textWidth(line)); }
    const int halfHeight = (MessageLineHeight * static_cast<int>(lines.size()) + MessagePadding) / 2;
    const int left = MessageCenterX - width / 2, top = MessageCenterY - halfHeight;
    // Final messages use a full-width black band with independently centered text.
    DrawRectangle(-1, top, ScreenWidth + 2, halfHeight * 2, BLACK);
    pixelBorder(-1, top, ScreenWidth + 2, halfHeight * 2 + 1, border);
    int visible = std::max(0, static_cast<int>((age - flow::MessageOpenSeconds) / flow::MessageCharacterSeconds) + 1);
    for (int row = 0; row < static_cast<int>(lines.size()); ++row) {
        const int count = std::min(visible, static_cast<int>(lines[row].size()));
        const auto fragment = lines[row].substr(0, count);
        text(fragment, left, top + MessageTextTop + row * MessageLineHeight);
        if (count > 0 && count < static_cast<int>(lines[row].size())) {
            DrawRectangle(left + textWidth(fragment) + 1, top + MessageTextTop - 1 + row * MessageLineHeight,
                          MessageCursorWidth, MessageCursorHeight, PixelGreen);
        }
        visible -= count;
    }
    continueArrow(MessageArrowY, 1);
}

int Renderer::richPage(const std::string& contents, int scroll) const {
    using namespace layout;
    ClearBackground(BLACK);
    DrawRectangle(0, PageTop, ScreenWidth, PageHeight, rgb(color::PageBackground));
    image("text_background", PageBackgroundX, PageBackgroundY);
    const auto titleEnd = contents.find('\n');
    const std::string title = contents.substr(0, titleEnd);
    const std::string body = contents.substr(titleEnd + 1);
    BeginScissorMode(PageTextLeft, PageTop, PageTextWidth, PageHeight);
    int cursor = PageFirstLine - scroll, alignment = 0;
    std::vector<int> lineOffsets;
    std::size_t position = 0;
    while (position <= body.size()) {
        const auto newline = body.find('\n', position);
        std::string line = body.substr(position, newline - position);
        int icon = -1;
        while (!line.empty() && line[0] == '<') {
            const auto end = line.find('>');
            if (end == std::string::npos) { break; }
            const auto tag = line.substr(0, end + 1);
            if (tag == "<align:left>") { alignment = 0; }
            else if (tag == "<align:center>") { alignment = 1; }
            else if (tag == "<align:right>") { alignment = 2; }
            else if (tag.rfind("<img:", 0) == 0) { icon = std::stoi(tag.substr(5)); }
            else { break; }
            line.erase(0, end + 1);
        }
        // Decode entities once; never interpret decoded text as markup again.
        std::string decoded;
        for (std::size_t i = 0; i < line.size();) {
            if (line.compare(i, 5, "&amp;") == 0) { decoded += '&'; i += 5; }
            else if (line.compare(i, 4, "&lt;") == 0) { decoded += '<'; i += 4; }
            else if (line.compare(i, 4, "&gt;") == 0) { decoded += '>'; i += 4; }
            else { decoded += line[i++]; }
        }
        int iconWidth = 0, lineHeight = GlyphHeight;
        Sprite iconImage;
        if (icon >= 0 && icon < static_cast<int>(assets_.helpIcons.size())) {
            iconImage = assets_.helpIcons[icon];
            iconWidth = iconImage.width + 1; lineHeight = iconImage.height;
        }
        do {
            lineOffsets.push_back(cursor + scroll - PageFirstLine);
            const int available = PageTextWidth - iconWidth;
            std::size_t count = decoded.size();
            while (count > 0 && textWidth(decoded.substr(0, count)) + 1 > available) { --count; }
            if (count < decoded.size()) {
                const auto space = decoded.find_last_of(" ,.", count);
                if (space != std::string::npos && space > 0) { count = space + 1; }
            }
            if (count == 0 && !decoded.empty()) { count = 1; }
            const auto fragment = decoded.substr(0, count);
            const int width = iconWidth + textWidth(fragment) + 1;
            int left = PageTextLeft;
            if (alignment == 1) { left = ScreenWidth / 2 - width / 2; }
            if (alignment == 2) { left = ScreenWidth - PageTextRightInset - width; }
            if (cursor + lineHeight <= PageTextBottom || (iconWidth > 0 && cursor + GlyphHeight <= PageTextBottom)) {
                if (iconWidth > 0) { sprite(iconImage, left, cursor, false); }
                text(fragment, left + iconWidth, cursor + (lineHeight - GlyphHeight) / 2);
            }
            cursor += lineHeight + PageLineGap;
            decoded.erase(0, count);
            iconWidth = 0; lineHeight = GlyphHeight;
        } while (!decoded.empty());
        if (newline == std::string::npos) { break; }
        position = newline + 1;
    }
    EndScissorMode();
    // Original help pages have black top and bottom bars with green corner gradients.
    DrawRectangle(0, 0, ScreenWidth, PageTop, BLACK);
    DrawRectangle(0, PageBottomBarY, ScreenWidth, PageBottomBarHeight, BLACK);
    for (int column = 0; column < PageGradientWidth; column += 2) {
        const Color shade{0, static_cast<unsigned char>(255 - column * 255 / PageGradientWidth), 0, 255};
        DrawRectangle(column, 0, 1, PageTop, shade); DrawRectangle(ScreenWidth - 1 - column, 0, 1, PageTop, shade);
        DrawRectangle(column, PageBottomBarY, 1, PageBottomBarHeight, shade);
        DrawRectangle(ScreenWidth - 1 - column, PageBottomBarY, 1, PageBottomBarHeight, shade);
    }
    pixelBorder(0, PageTop - 1, ScreenWidth, PageHeight + 2, PixelGreen);
    text(title, ScreenWidth / 2 - (textWidth(title) + 1) / 2, PageTitleY);
    // Up and down arrows with dotted stems at both ends of the scroll track.
    const int arrowX = PageScrollX;
    pixelTriangle(arrowX, PageTop + 2, 1, 1, PixelGreen);
    pixelLine(arrowX - 1, PageTop + 3, arrowX + 1, PageTop + 3, PixelGreen); pixelLine(arrowX - 2, PageTop + 4, arrowX + 2, PageTop + 4, PixelGreen);
    pixelLine(arrowX, PageTextBottom, arrowX, PageTextBottom, PixelGreen);
    pixelLine(arrowX - 1, PageTextBottom - 1, arrowX + 1, PageTextBottom - 1, PixelGreen);
    pixelLine(arrowX - 2, PageTextBottom - 2, arrowX + 2, PageTextBottom - 2, PixelGreen);
    for (int dot = 0; dot < 3; ++dot) {
        DrawPixel(arrowX, PageTop + 6 + dot * 2, PixelGreen); DrawPixel(arrowX, PageTextBottom - 4 - dot * 2, PixelGreen);
    }
    const int contentHeight = cursor + scroll - PageContentMargin;
    // Scroll whole rows and expose the entire last row, not merely the scrollbar's nominal range.
    int maximum = 0;
    const int required = contentHeight - PageLastRowReach;
    for (int offset : lineOffsets) {
        if (offset >= required) { maximum = offset; break; }
    }
    const int track = PageScrollTrackHeight;
    const int thumb = std::clamp(track * track / std::max(1, contentHeight), PageScrollMinThumb, track);
    int thumbY = PageScrollTrackTop;
    if (contentHeight > PageScrollableHeight) { thumbY += std::min(track - thumb, (track - thumb) * scroll / (contentHeight - PageScrollableHeight)); }
    BeginScissorMode(arrowX - 2, PageScrollTrackTop, 5, track);
    pixelLine(arrowX - 1, thumbY, arrowX + 1, thumbY, PixelGreen); thumbY += 2;
    for (int height = 4; height < thumb; height += 2) { pixelLine(arrowX - 2, thumbY, arrowX + 2, thumbY, PixelGreen); thumbY += 2; }
    pixelLine(arrowX - 1, thumbY, arrowX + 1, thumbY, PixelGreen);
    EndScissorMode();
    return maximum;
}
}
