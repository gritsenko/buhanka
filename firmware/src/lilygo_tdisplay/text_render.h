#pragma once

#include <cstdint>

class TFT_eSprite;

namespace road_east::ui {

// Width of one glyph cell in pixels (advance width).
int textGlyphWidth();
int textGlyphHeight();

// Width in pixels of `utf8` rendered with the bundled 8x16 font.
int textWidth(const char* utf8);

// Draw `utf8` starting at (x, y) with foreground color `fg`.
// If `bgTransparent` is false, background pixels are filled with `bg`.
// Returns advance in pixels.
int drawTextUtf8(TFT_eSprite& dst,
                 int x,
                 int y,
                 const char* utf8,
                 uint16_t fg,
                 uint16_t bg = 0,
                 bool bgTransparent = true);

enum class TextAnchor {
    TopLeft,
    TopCenter,
    MiddleCenter,
};

// Draw text anchored relative to (x, y).
void drawTextAnchored(TFT_eSprite& dst,
                      int x,
                      int y,
                      const char* utf8,
                      uint16_t fg,
                      TextAnchor anchor,
                      uint16_t bg = 0,
                      bool bgTransparent = true);

}  // namespace road_east::ui
