// Initial firmware for "Дорога на восток" on LilyGO TTGO T-Display v1.1.
//
// Hardware:
//   - ST7789 135x240 TFT, used in landscape (240x135 after rotation 1)
//   - Two user buttons: GPIO 0 and GPIO 35
//   - Backlight on GPIO 4
//
// Splash bitmap and bitmap font with Cyrillic glyphs are generated from
// assets/sprites/splash.png and a TTF by scripts/build_firmware_assets.py
// into firmware/include/generated/.

#include <Arduino.h>
#include <TFT_eSPI.h>

#include "generated/font_8x16.h"
#include "generated/splash_image.h"
#include "road_east_version.h"
#include "text_render.h"

namespace {

constexpr int kScreenW = 240;
constexpr int kScreenH = 135;

constexpr int kBacklightPin = 4;
// In landscape (rotation 1) the buttons sit on the right edge of the screen.
// We don't bake any "left/right" semantics into the names — the firmware just
// labels them by function (select vs. switch).
constexpr int kBtnSelectPin = 0;
constexpr int kBtnSwitchPin = 35;

constexpr uint32_t kFrameMs = 33;
constexpr uint32_t kSplashMinMs = 800;

enum class Scene {
    Splash,
    Menu,
};

struct Button {
    int pin;
    bool down = false;
    bool prev = false;

    explicit Button(int p) : pin(p) {}
    bool justPressed() const { return down && !prev; }
};

TFT_eSPI tft;
TFT_eSprite frame = TFT_eSprite(&tft);

Scene scene = Scene::Splash;
uint32_t splashStartedAt = 0;
uint32_t lastFrameAt = 0;

Button btnSelect{kBtnSelectPin};
Button btnSwitch{kBtnSwitchPin};

int menuCursor = 0;
constexpr int kMenuItemCount = 3;
const char* const kMenuItems[kMenuItemCount] = {
    "Новая поездка",
    "Продолжить",
    "Об игре",
};

void setBacklight(bool on) {
    pinMode(kBacklightPin, OUTPUT);
    digitalWrite(kBacklightPin, on ? HIGH : LOW);
}

void readButton(Button& b) {
    b.prev = b.down;
    b.down = digitalRead(b.pin) == LOW;
}

// Blit the 4bpp packed splash bitmap into the back-buffer sprite, decoding
// one row at a time through a small RGB565 line buffer.
void blitSplashBitmap() {
    using namespace road_east::assets;
    static uint16_t rowBuf[kSplashWidth];
    for (int y = 0; y < kSplashHeight; ++y) {
        const uint8_t* src = &kSplashPixels[y * (kSplashWidth / 2)];
        for (int x = 0; x < kSplashWidth; x += 2) {
            const uint8_t byte = src[x / 2];
            rowBuf[x] = kSplashPalette[byte >> 4];
            rowBuf[x + 1] = kSplashPalette[byte & 0x0F];
        }
        frame.pushImage(0, y, kSplashWidth, 1, rowBuf);
    }
}

void drawSplash(uint32_t now) {
    blitSplashBitmap();

    const bool blink = ((now / 500) % 2) == 0;
    if (blink) {
        // Subtle prompt over the artwork: a small dark band keeps it legible.
        const int promptY = kScreenH - 18;
        frame.fillRect(0, promptY - 2, kScreenW, 18, TFT_BLACK);
        road_east::ui::drawTextAnchored(
            frame, kScreenW / 2, promptY, "Нажмите кнопку",
            TFT_YELLOW, road_east::ui::TextAnchor::TopCenter);
    }

    frame.pushSprite(0, 0);
}

void drawMenu() {
    frame.fillSprite(TFT_BLACK);

    // Title bar.
    frame.fillRect(0, 0, kScreenW, 22, TFT_NAVY);
    road_east::ui::drawTextAnchored(
        frame, kScreenW / 2, 3, "ДОРОГА НА ВОСТОК",
        TFT_WHITE, road_east::ui::TextAnchor::TopCenter);

    const int itemTop = 36;
    const int itemH = 22;
    for (int i = 0; i < kMenuItemCount; ++i) {
        const bool selected = (i == menuCursor);
        const int y = itemTop + i * itemH;
        const uint16_t fg = selected ? TFT_WHITE : TFT_SILVER;
        if (selected) {
            frame.fillRoundRect(6, y - 2, kScreenW - 12, 20, 4, TFT_DARKGREEN);
        }
        road_east::ui::drawTextAnchored(
            frame, kScreenW / 2, y, kMenuItems[i], fg,
            road_east::ui::TextAnchor::TopCenter);
    }

    // Footer with control hint + version.
    road_east::ui::drawTextAnchored(
        frame, kScreenW / 2, kScreenH - 18,
        "Верх: выбор   Низ: далее",
        TFT_DARKGREY, road_east::ui::TextAnchor::TopCenter);

    frame.pushSprite(0, 0);
}

void updateSplash(uint32_t now) {
    const bool anyEdge = btnSelect.justPressed() || btnSwitch.justPressed();
    if (anyEdge && now - splashStartedAt >= kSplashMinMs) {
        scene = Scene::Menu;
    }
    drawSplash(now);
}

void updateMenu() {
    if (btnSwitch.justPressed()) {
        menuCursor = (menuCursor + 1) % kMenuItemCount;
    }
    if (btnSelect.justPressed()) {
        Serial.print("[menu] selected: ");
        Serial.println(kMenuItems[menuCursor]);
    }
    drawMenu();
}

}  // namespace

void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println();
    Serial.print(road_east::kFirmwareName);
    Serial.print(" ");
    Serial.println(road_east::kFirmwareVersion);

    setBacklight(true);

    pinMode(kBtnSelectPin, INPUT_PULLUP);
    pinMode(kBtnSwitchPin, INPUT);

    tft.init();
    tft.setRotation(1);

    frame.setColorDepth(16);
    frame.createSprite(kScreenW, kScreenH);
    // pushImage(uint16_t*) memcpys into the sprite buffer, but TFT_eSprite
    // stores 16-bit pixels pre-byte-swapped (fillRect/drawPixel apply the
    // swap themselves). Without setSwapBytes the splash palette comes out
    // with its high/low bytes reversed — orange becomes purple, green
    // becomes blue, etc.
    frame.setSwapBytes(true);
    frame.fillSprite(TFT_BLACK);
    frame.pushSprite(0, 0);

    splashStartedAt = millis();
    lastFrameAt = splashStartedAt;
}

void loop() {
    const uint32_t now = millis();
    if (now - lastFrameAt < kFrameMs) {
        delay(1);
        return;
    }
    lastFrameAt = now;

    readButton(btnSelect);
    readButton(btnSwitch);

    switch (scene) {
        case Scene::Splash:
            updateSplash(now);
            break;
        case Scene::Menu:
            updateMenu();
            break;
    }
}
