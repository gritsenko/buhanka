#include "text_render.h"

#include "generated/font_6x12.h"
#include "generated/font_8x16.h"

namespace road_east::ui {

namespace {

using namespace road_east::assets;

// Punctuation appended after ё — must match FONT_EXTRA in
// scripts/build_firmware_assets.py, same order.
constexpr uint32_t kExtraCodepoints[] = {
    0x00AB,  // «
    0x00BB,  // »
    0x2013,  // –
    0x2014,  // —
    0x2018,  // ‘
    0x2019,  // ’
    0x201C,  // “
    0x201D,  // ”
    0x2026,  // …
};
constexpr int kExtraBase = 161;

// Map a Unicode codepoint to its index in the glyph table. Layout matches
// FONT_CODEPOINTS in scripts/build_firmware_assets.py.
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
    for (int i = 0; i < static_cast<int>(sizeof(kExtraCodepoints) / sizeof(uint32_t)); ++i) {
        if (kExtraCodepoints[i] == cp) return kExtraBase + i;
    }
    return 0;  // fallback = space
}

// Decode one UTF-8 codepoint and advance past it. Returns `s` unchanged at the
// terminating NUL so callers can loop on `*s`.
const char* decodeUtf8(const char* s, uint32_t& cp) {
    const uint8_t b0 = static_cast<uint8_t>(s[0]);
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

int countGlyphsN(const char* utf8, int bytes) {
    int n = 0;
    const char* end = utf8 + bytes;
    uint32_t cp = 0;
    while (utf8 < end && *utf8) {
        utf8 = decodeUtf8(utf8, cp);
        ++n;
    }
    return n;
}

void drawGlyph(gfx::Canvas& canvas, const Font& font, int x, int y, int idx, uint16_t fg) {
    if (idx < 0 || idx >= font.glyphCount) idx = 0;
    const uint8_t* rows = &font.glyphs[idx * font.height];
    for (int dy = 0; dy < font.height; ++dy) {
        const uint8_t bits = rows[dy];
        if (bits == 0) continue;
        for (int dx = 0; dx < font.cellWidth; ++dx) {
            if (bits & (0x80 >> dx)) {
                canvas.drawPixel(x + dx, y + dy, fg);
            }
        }
    }
}

}  // namespace

const Font& fontLarge() {
    static const Font font{kFont8x16GlyphWidth, kFont8x16GlyphAdvance,
                           kFont8x16GlyphHeight, kFont8x16GlyphCount,
                           kFont8x16Glyphs};
    return font;
}

const Font& fontSmall() {
    static const Font font{kFont6x12GlyphWidth, kFont6x12GlyphAdvance,
                           kFont6x12GlyphHeight, kFont6x12GlyphCount,
                           kFont6x12Glyphs};
    return font;
}

int textWidthN(const Font& font, const char* utf8, int bytes) {
    return countGlyphsN(utf8, bytes) * font.advance;
}

int textWidth(const Font& font, const char* utf8) {
    if (utf8 == nullptr) return 0;
    return textWidthN(font, utf8, 0x7FFF);
}

int charsPerWidth(const Font& font, int maxWidth) {
    if (font.advance <= 0) return 0;
    const int n = maxWidth / font.advance;
    return n > 0 ? n : 0;
}

int drawTextN(gfx::Canvas& canvas, const Font& font, int x, int y,
              const char* utf8, int bytes, uint16_t fg) {
    if (utf8 == nullptr) return 0;
    const char* end = utf8 + bytes;
    int cursor = x;
    uint32_t cp = 0;
    while (utf8 < end && *utf8) {
        utf8 = decodeUtf8(utf8, cp);
        drawGlyph(canvas, font, cursor, y, glyphIndex(cp), fg);
        cursor += font.advance;
    }
    return cursor - x;
}

int drawText(gfx::Canvas& canvas, const Font& font, int x, int y,
             const char* utf8, uint16_t fg) {
    return drawTextN(canvas, font, x, y, utf8, 0x7FFF, fg);
}

void drawTextAnchoredN(gfx::Canvas& canvas, const Font& font, int x, int y,
                       const char* utf8, int bytes, uint16_t fg, TextAnchor anchor) {
    const int w = textWidthN(font, utf8, bytes);
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
            oy = y - font.height / 2;
            break;
    }
    drawTextN(canvas, font, ox, oy, utf8, bytes, fg);
}

void drawTextAnchored(gfx::Canvas& canvas, const Font& font, int x, int y,
                      const char* utf8, uint16_t fg, TextAnchor anchor) {
    drawTextAnchoredN(canvas, font, x, y, utf8, 0x7FFF, fg, anchor);
}

int wrapText(const Font& font, const char* utf8, int maxWidth,
             TextLine* out, int maxLines, bool* truncated) {
    if (truncated) *truncated = false;
    if (utf8 == nullptr || out == nullptr || maxLines <= 0) return 0;

    const int maxChars = charsPerWidth(font, maxWidth);
    if (maxChars <= 0) return 0;

    int lines = 0;
    const char* p = utf8;
    const char* lineStart = p;
    const char* lastSpace = nullptr;  // points at a ' ' inside the current line
    int glyphs = 0;

    // Records a finished line; returns false once the caller's buffer is full.
    auto emit = [&](const char* begin, const char* end) -> bool {
        if (lines >= maxLines) {
            if (truncated) *truncated = true;
            return false;
        }
        out[lines].begin = begin;
        out[lines].bytes = static_cast<uint16_t>(end - begin);
        ++lines;
        return true;
    };

    while (*p) {
        uint32_t cp = 0;
        const char* next = decodeUtf8(p, cp);

        if (cp == '\n') {
            if (!emit(lineStart, p)) return lines;
            p = next;
            lineStart = p;
            lastSpace = nullptr;
            glyphs = 0;
            continue;
        }

        if (cp == ' ') lastSpace = p;

        if (glyphs >= maxChars) {
            if (lastSpace != nullptr && lastSpace > lineStart) {
                if (!emit(lineStart, lastSpace)) return lines;
                p = lastSpace + 1;  // the break character itself is dropped
            } else {
                if (!emit(lineStart, p)) return lines;
            }
            lineStart = p;
            lastSpace = nullptr;
            glyphs = 0;
            continue;
        }

        ++glyphs;
        p = next;
    }

    if (p > lineStart || lines == 0) {
        emit(lineStart, p);
    }
    return lines;
}

}  // namespace road_east::ui
