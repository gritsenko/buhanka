#include "canvas.h"

namespace road_east::gfx {

namespace {

inline int clampInt(int v, int lo, int hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

}  // namespace

Canvas::Canvas(uint16_t* pixels, int width, int height)
    : pixels_(pixels), width_(width), height_(height) {
    resetClip();
}

void Canvas::setClip(int x, int y, int w, int h) {
    clipX0_ = clampInt(x, 0, width_);
    clipY0_ = clampInt(y, 0, height_);
    clipX1_ = clampInt(x + w, clipX0_, width_);
    clipY1_ = clampInt(y + h, clipY0_, height_);
}

void Canvas::resetClip() {
    clipX0_ = 0;
    clipY0_ = 0;
    clipX1_ = width_;
    clipY1_ = height_;
}

void Canvas::clear(uint16_t color) {
    fillRect(0, 0, width_, height_, color);
}

void Canvas::drawPixel(int x, int y, uint16_t color) {
    if (x < clipX0_ || x >= clipX1_ || y < clipY0_ || y >= clipY1_) return;
    pixels_[y * width_ + x] = color;
}

void Canvas::fillRect(int x, int y, int w, int h, uint16_t color) {
    const int x0 = clampInt(x, clipX0_, clipX1_);
    const int y0 = clampInt(y, clipY0_, clipY1_);
    const int x1 = clampInt(x + w, clipX0_, clipX1_);
    const int y1 = clampInt(y + h, clipY0_, clipY1_);
    for (int py = y0; py < y1; ++py) {
        uint16_t* row = pixels_ + py * width_;
        for (int px = x0; px < x1; ++px) {
            row[px] = color;
        }
    }
}

void Canvas::drawRect(int x, int y, int w, int h, uint16_t color) {
    if (w <= 0 || h <= 0) return;
    drawHLine(x, y, w, color);
    drawHLine(x, y + h - 1, w, color);
    drawVLine(x, y, h, color);
    drawVLine(x + w - 1, y, h, color);
}

void Canvas::drawHLine(int x, int y, int w, uint16_t color) {
    fillRect(x, y, w, 1, color);
}

void Canvas::drawVLine(int x, int y, int h, uint16_t color) {
    fillRect(x, y, 1, h, color);
}

void Canvas::drawDashedHLine(int x, int y, int w, int on, int off, uint16_t color) {
    if (on <= 0 || off < 0) return;
    for (int px = 0; px < w; px += on + off) {
        const int len = (px + on <= w) ? on : (w - px);
        fillRect(x + px, y, len, 1, color);
    }
}

// Rounded rectangles are filled row by row: for the `r` rows at each end the
// row is inset by the horizontal chord of the corner circle. Cheap, and the
// result is identical to the CSS border-radius look at these sizes.
void Canvas::fillRoundRect(int x, int y, int w, int h, int r, uint16_t color) {
    if (w <= 0 || h <= 0) return;
    if (r * 2 > w) r = w / 2;
    if (r * 2 > h) r = h / 2;
    if (r <= 0) {
        fillRect(x, y, w, h, color);
        return;
    }
    for (int row = 0; row < h; ++row) {
        int inset = 0;
        const int dy = (row < r) ? (r - 1 - row) : ((row >= h - r) ? (row - (h - r)) : -1);
        if (dy >= 0) {
            // Largest dx with dx^2 + dy^2 <= (r-1)^2 gives the chord half-width.
            int dx = r - 1;
            while (dx > 0 && dx * dx + dy * dy > (r - 1) * (r - 1)) --dx;
            inset = (r - 1) - dx;
        }
        fillRect(x + inset, y + row, w - inset * 2, 1, color);
    }
}

void Canvas::drawRoundRect(int x, int y, int w, int h, int r, uint16_t color) {
    if (w <= 0 || h <= 0) return;
    if (r * 2 > w) r = w / 2;
    if (r * 2 > h) r = h / 2;
    if (r <= 0) {
        drawRect(x, y, w, h, color);
        return;
    }
    drawHLine(x + r, y, w - 2 * r, color);
    drawHLine(x + r, y + h - 1, w - 2 * r, color);
    drawVLine(x, y + r, h - 2 * r, color);
    drawVLine(x + w - 1, y + r, h - 2 * r, color);
    // Corner arcs, one pixel per row of the quadrant.
    for (int dy = 0; dy < r; ++dy) {
        int dx = r - 1;
        while (dx > 0 && dx * dx + dy * dy > (r - 1) * (r - 1)) --dx;
        const int inset = (r - 1) - dx;
        drawPixel(x + inset, y + (r - 1 - dy), color);
        drawPixel(x + w - 1 - inset, y + (r - 1 - dy), color);
        drawPixel(x + inset, y + h - r + dy, color);
        drawPixel(x + w - 1 - inset, y + h - r + dy, color);
    }
}

void Canvas::blitSprite(const data::SpriteDef& sprite, int frame, int x, int y) {
    if (sprite.frames <= 0) return;
    if (frame < 0 || frame >= sprite.frames) frame = 0;

    const int fw = sprite.frameWidth;
    const int fh = sprite.frameHeight;
    const int srcX = frame * fw;
    const int maskStride = (sprite.sheetWidth + 7) / 8;

    // Clip up front so the inner loop stays branch-light.
    const int dx0 = clampInt(x, clipX0_, clipX1_);
    const int dy0 = clampInt(y, clipY0_, clipY1_);
    const int dx1 = clampInt(x + fw, clipX0_, clipX1_);
    const int dy1 = clampInt(y + fh, clipY0_, clipY1_);

    for (int py = dy0; py < dy1; ++py) {
        const int sy = py - y;
        const uint16_t* srcRow = sprite.pixels + sy * sprite.sheetWidth;
        const uint8_t* maskRow = sprite.mask + sy * maskStride;
        uint16_t* dstRow = pixels_ + py * width_;
        for (int px = dx0; px < dx1; ++px) {
            const int sx = srcX + (px - x);
            if ((maskRow[sx >> 3] & (0x80 >> (sx & 7))) == 0) continue;
            dstRow[px] = srcRow[sx];
        }
    }
}

void Canvas::blitMono(const uint8_t* rows, int w, int h, int x, int y, uint16_t color) {
    const int stride = (w + 7) / 8;
    for (int sy = 0; sy < h; ++sy) {
        const uint8_t* row = rows + sy * stride;
        for (int sx = 0; sx < w; ++sx) {
            if ((row[sx >> 3] & (0x80 >> (sx & 7))) == 0) continue;
            drawPixel(x + sx, y + sy, color);
        }
    }
}

}  // namespace road_east::gfx
