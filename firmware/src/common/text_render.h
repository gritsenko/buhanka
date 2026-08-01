// UTF-8 bitmap text rendering on a Canvas.
//
// Two fonts are baked into the firmware by scripts/build_firmware_assets.py:
// an 8x16 face for headings and a 6x12 face for body copy — 240px of screen
// only fits 30 characters at 8px, which is not enough for the event texts the
// PWA prototype shows.
//
// Supported codepoints: ASCII 0x20..0x7E, U+0410..U+044F and Ё/ё. Anything
// else renders as a space (extend FONT_CODEPOINTS in the generator and
// glyphIndex() here together).

#pragma once

#include <cstdint>

#include "canvas.h"

namespace road_east::ui {

struct Font {
    // Bitmap cell width. May exceed `advance`: a few glyphs ('0', ы, ю, щ)
    // need one column more than the step, and they are allowed to bleed into
    // the next cell rather than be clipped into a different letter.
    int cellWidth;
    int advance;  // cursor step per character, in pixels
    int height;   // cell height, in pixels
    int glyphCount;
    const uint8_t* glyphs;  // `height` bytes per glyph, one byte per row, MSB = leftmost
};

const Font& fontLarge();  // 8x16
const Font& fontSmall();  // 6x12

// Width in pixels of `utf8` rendered with `font`.
int textWidth(const Font& font, const char* utf8);
int textWidthN(const Font& font, const char* utf8, int bytes);

// Number of characters that fit into `maxWidth` pixels.
int charsPerWidth(const Font& font, int maxWidth);

int drawText(gfx::Canvas& canvas, const Font& font, int x, int y,
             const char* utf8, uint16_t fg);
int drawTextN(gfx::Canvas& canvas, const Font& font, int x, int y,
              const char* utf8, int bytes, uint16_t fg);

enum class TextAnchor {
    TopLeft,
    TopCenter,
    MiddleCenter,
};

void drawTextAnchored(gfx::Canvas& canvas, const Font& font, int x, int y,
                      const char* utf8, uint16_t fg, TextAnchor anchor);
void drawTextAnchoredN(gfx::Canvas& canvas, const Font& font, int x, int y,
                       const char* utf8, int bytes, uint16_t fg, TextAnchor anchor);

// One wrapped line: a slice of the source string (never NUL-terminated).
struct TextLine {
    const char* begin;
    uint16_t bytes;
};

// Greedy word wrap at `maxWidth` pixels, honouring '\n'. Writes at most
// `maxLines` entries and returns how many were produced; the return value is
// clamped to `maxLines`, so `truncated` tells you whether text was dropped.
int wrapText(const Font& font, const char* utf8, int maxWidth,
             TextLine* out, int maxLines, bool* truncated = nullptr);

}  // namespace road_east::ui
