#include "text_render.h"

#include <TFT_eSPI.h>
#include <cstring>

#include "generated/font_8x16.h"

namespace road_east::ui {

namespace {

using namespace road_east::assets;

// Map a Unicode codepoint to its index in kFontGlyphs. Layout matches the
// generator in scripts/build_firmware_assets.py.
int glyphIndex(uint32_t cp) {
    if (cp >= 0x20 && cp <= 0x7E) {
        return static_cast<int>(cp - 0x20);
    }
    if (cp == 0x401) {  // Ё
        return 95;
    }
    if (cp >= 0x410 && cp <= 0x44F) {  // А..я
        return 96 + static_cast<int>(cp - 0x410);
    }
    if (cp == 0x451) {  // ё
        return 160;
    }
    return 0;  // fallback = space
}

// Decode one UTF-8 codepoint, advance `s`. Returns 0 at end of string;
// caller stops on '\0'.
const char* decodeUtf8(const char* s, uint32_t& cp) {
    uint8_t b0 = static_cast<uint8_t>(*s);
    if (b0 < 0x80) {
        cp = b0;
        return s + (b0 ? 1 : 0);
    }
    if ((b0 & 0xE0) == 0xC0 && (s[1] & 0xC0) == 0x80) {
        cp = ((b0 & 0x1F) << 6) | (static_cast<uint8_t>(s[1]) & 0x3F);
        return s + 2;
    }
    if ((b0 & 0xF0) == 0xE0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80) {
        cp = ((b0 & 0x0F) << 12) |
             ((static_cast<uint8_t>(s[1]) & 0x3F) << 6) |
             (static_cast<uint8_t>(s[2]) & 0x3F);
        return s + 3;
    }
    if ((b0 & 0xF8) == 0xF0 && (s[1] & 0xC0) == 0x80 &&
        (s[2] & 0xC0) == 0x80 && (s[3] & 0xC0) == 0x80) {
        cp = ((b0 & 0x07) << 18) |
             ((static_cast<uint8_t>(s[1]) & 0x3F) << 12) |
             ((static_cast<uint8_t>(s[2]) & 0x3F) << 6) |
             (static_cast<uint8_t>(s[3]) & 0x3F);
        return s + 4;
    }
    cp = '?';
    return s + 1;
}

int countGlyphs(const char* utf8) {
    int n = 0;
    uint32_t cp = 0;
    while (*utf8) {
        utf8 = decodeUtf8(utf8, cp);
        if (cp == 0) break;
        ++n;
    }
    return n;
}

void drawGlyph(TFT_eSprite& dst, int x, int y, int idx,
               uint16_t fg, uint16_t bg, bool bgTransparent) {
    const uint8_t* rows = &kFontGlyphs[idx * kFontGlyphHeight];
    for (int dy = 0; dy < kFontGlyphHeight; ++dy) {
        uint8_t bits = rows[dy];
        for (int dx = 0; dx < kFontGlyphWidth; ++dx) {
            const bool on = (bits & (0x80 >> dx)) != 0;
            if (on) {
                dst.drawPixel(x + dx, y + dy, fg);
            } else if (!bgTransparent) {
                dst.drawPixel(x + dx, y + dy, bg);
            }
        }
    }
}

}  // namespace

int textGlyphWidth() { return kFontGlyphWidth; }
int textGlyphHeight() { return kFontGlyphHeight; }

int textWidth(const char* utf8) {
    return countGlyphs(utf8) * kFontGlyphWidth;
}

int drawTextUtf8(TFT_eSprite& dst, int x, int y, const char* utf8,
                 uint16_t fg, uint16_t bg, bool bgTransparent) {
    int cursor = x;
    uint32_t cp = 0;
    while (*utf8) {
        utf8 = decodeUtf8(utf8, cp);
        if (cp == 0) break;
        drawGlyph(dst, cursor, y, glyphIndex(cp), fg, bg, bgTransparent);
        cursor += kFontGlyphWidth;
    }
    return cursor - x;
}

void drawTextAnchored(TFT_eSprite& dst, int x, int y, const char* utf8,
                      uint16_t fg, TextAnchor anchor,
                      uint16_t bg, bool bgTransparent) {
    const int w = textWidth(utf8);
    int ox = x;
    int oy = y;
    switch (anchor) {
        case TextAnchor::TopLeft:
            break;
        case TextAnchor::TopCenter:
            ox = x - w / 2;
            break;
        case TextAnchor::MiddleCenter:
            ox = x - w / 2;
            oy = y - kFontGlyphHeight / 2;
            break;
    }
    drawTextUtf8(dst, ox, oy, utf8, fg, bg, bgTransparent);
}

}  // namespace road_east::ui
