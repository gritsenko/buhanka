// Device-independent RGB565 drawing surface.
//
// Everything the game draws goes through a Canvas, so the whole UI/scene layer
// stays free of TFT_eSPI (and of Arduino). The device client owns the pixel
// buffer and pushes it to its panel once per frame.

#pragma once

#include <cstdint>

#include "game_data_types.h"

namespace road_east::gfx {

constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

// Convert a 0xRRGGBB literal. Handy for palettes lifted from css/style.css.
constexpr uint16_t rgb565Hex(uint32_t rgb) {
    return rgb565(static_cast<uint8_t>((rgb >> 16) & 0xFF),
                  static_cast<uint8_t>((rgb >> 8) & 0xFF),
                  static_cast<uint8_t>(rgb & 0xFF));
}

class Canvas {
public:
    Canvas(uint16_t* pixels, int width, int height);

    int width() const { return width_; }
    int height() const { return height_; }
    uint16_t* pixels() const { return pixels_; }

    // Drawing is clipped to this rectangle (intersected with the surface).
    void setClip(int x, int y, int w, int h);
    void resetClip();

    void clear(uint16_t color);
    void drawPixel(int x, int y, uint16_t color);
    void fillRect(int x, int y, int w, int h, uint16_t color);
    void drawRect(int x, int y, int w, int h, uint16_t color);
    void fillRoundRect(int x, int y, int w, int h, int r, uint16_t color);
    void drawRoundRect(int x, int y, int w, int h, int r, uint16_t color);
    void drawHLine(int x, int y, int w, uint16_t color);
    void drawVLine(int x, int y, int h, uint16_t color);
    // Horizontal dashed rule, `on` px drawn then `off` px skipped.
    void drawDashedHLine(int x, int y, int w, int on, int off, uint16_t color);

    // Blit one frame of a sprite sheet with its 1bpp transparency mask.
    void blitSprite(const data::SpriteDef& sprite, int frame, int x, int y);

    // Blit a 1bpp bitmap (MSB-first rows) as a solid-colour icon.
    void blitMono(const uint8_t* rows, int w, int h, int x, int y, uint16_t color);

private:
    uint16_t* pixels_;
    int width_;
    int height_;
    int clipX0_;
    int clipY0_;
    int clipX1_;  // exclusive
    int clipY1_;  // exclusive
};

}  // namespace road_east::gfx
