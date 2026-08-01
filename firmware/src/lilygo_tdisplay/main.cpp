// Hardware client for "Дорога на восток" on LilyGO TTGO T-Display v1.1.
//
// This file owns only the platform: display bring-up, the two buttons and the
// frame pump. Game rules, layout and the pixel-art scenes live in src/common/
// and draw into a plain RGB565 Canvas, which is pushed to the panel here.
//
// Hardware:
//   - ST7789 135x240 TFT, used in landscape (240x135 after rotation 1)
//   - Two user buttons: GPIO 0 and GPIO 35
//   - Backlight on GPIO 4

#include <Arduino.h>
#include <TFT_eSPI.h>

#include "canvas.h"
#include "game.h"
#include "road_east_version.h"
#include "ui.h"

namespace {

constexpr int kScreenW = road_east::ui::kScreenWidth;
constexpr int kScreenH = road_east::ui::kScreenHeight;

constexpr int kBacklightPin = 4;
// In landscape (rotation 1) GPIO 35 is the upper button and GPIO 0 the lower
// one, matching the #btn-top / #btn-bottom pair of the PWA prototype.
constexpr int kBtnTopPin = 35;     // "ВЫБОР": short — next choice, long — status
constexpr int kBtnBottomPin = 0;   // "ОК":    short — confirm,     long — map

constexpr uint32_t kFrameMs = 33;
constexpr uint32_t kSplashMinMs = 800;
// setupButton() in js/app.js treats anything past 600 ms as a long press.
constexpr uint32_t kLongPressMs = 600;

enum class Screen {
    Splash,
    Menu,
    About,
    Game,
};

struct Button {
    int pin;
    bool down = false;
    bool prev = false;
    uint32_t pressedAt = 0;
    bool longFired = false;
    bool armed = false;  // false when the press started while input was blocked

    explicit Button(int p) : pin(p) {}
    bool justPressed() const { return down && !prev; }
    bool justReleased() const { return !down && prev; }
};

TFT_eSPI tft;

// The one and only framebuffer: 240 * 135 * 2 = 64800 bytes of DRAM.
uint16_t frameBuffer[kScreenW * kScreenH];
road_east::gfx::Canvas canvas(frameBuffer, kScreenW, kScreenH);

road_east::game::Game game;

Screen screen = Screen::Splash;
uint32_t splashStartedAt = 0;
uint32_t lastFrameAt = 0;

Button btnTop{kBtnTopPin};
Button btnBottom{kBtnBottomPin};

constexpr int kMenuItemCount = 3;
int menuCursor = 0;
bool runStarted = false;  // enables "Продолжить"

void setBacklight(bool on) {
    pinMode(kBacklightPin, OUTPUT);
    digitalWrite(kBacklightPin, on ? HIGH : LOW);
}

void readButton(Button& b) {
    b.prev = b.down;
    b.down = digitalRead(b.pin) == LOW;
}

void activateMenuItem(uint32_t now) {
    switch (menuCursor) {
        case 0:
            game.reset(now);
            runStarted = true;
            screen = Screen::Game;
            break;
        case 1:
            if (runStarted) screen = Screen::Game;
            break;
        default:
            screen = Screen::About;
            break;
    }
}

// Mirrors setupButton() in js/app.js: a press is ignored outright while a
// transition plays, a hold past kLongPressMs fires the long action once, and a
// release either dismisses the result card or runs the short action.
void updateGameButton(Button& b, uint32_t now, bool isTopButton) {
    using road_east::game::Overlay;
    using road_east::game::Phase;

    if (b.justPressed()) {
        b.armed = !game.busy();
        b.longFired = false;
        b.pressedAt = now;
    }

    if (b.down && b.armed && !b.longFired && game.phase() != Phase::Result &&
        now - b.pressedAt >= kLongPressMs) {
        b.longFired = true;
        if (isTopButton) {
            // Second long press on the status overlay leaves for the menu —
            // the only way back on hardware that has just two buttons.
            if (game.overlay() == Overlay::Status) {
                game.closeOverlay();
                screen = Screen::Menu;
            } else {
                game.openStatus();
            }
        } else {
            game.openMap();
        }
    }

    if (b.justReleased()) {
        const bool armed = b.armed;
        b.armed = false;
        if (!armed) return;
        if (game.phase() == Phase::Result) {
            game.confirm(now);
            return;
        }
        if (b.longFired) return;
        if (isTopButton) {
            game.selectNext();
        } else {
            game.confirm(now);
        }
    }
}

void updateSplash(uint32_t now) {
    const bool anyEdge = btnTop.justPressed() || btnBottom.justPressed();
    if (anyEdge && now - splashStartedAt >= kSplashMinMs) {
        screen = Screen::Menu;
    }
    road_east::ui::drawSplash(canvas, now);
}

void updateMenu(uint32_t now) {
    if (btnTop.justPressed()) {
        menuCursor = (menuCursor + 1) % kMenuItemCount;
    }
    if (btnBottom.justPressed()) {
        activateMenuItem(now);
    }
    road_east::ui::drawMenu(canvas, menuCursor, runStarted);
}

void updateAbout() {
    if (btnTop.justPressed() || btnBottom.justPressed()) {
        screen = Screen::Menu;
    }
    road_east::ui::drawAbout(canvas);
}

void updateGame(uint32_t now) {
    updateGameButton(btnTop, now, true);
    updateGameButton(btnBottom, now, false);
    game.tick(now);
    road_east::ui::drawGame(canvas, game, now);
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

    // GPIO 35 has no internal pull-up on this revision; the board provides one.
    pinMode(kBtnTopPin, INPUT);
    pinMode(kBtnBottomPin, INPUT_PULLUP);

    tft.init();
    tft.setRotation(1);
    // The canvas stores RGB565 in natural byte order; the panel wants it
    // swapped. Without this the palette comes out with the bytes reversed —
    // orange turns purple, green turns blue.
    tft.setSwapBytes(true);
    tft.fillScreen(TFT_BLACK);

    game.reset(millis());

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

    readButton(btnTop);
    readButton(btnBottom);

    switch (screen) {
        case Screen::Splash:
            updateSplash(now);
            break;
        case Screen::Menu:
            updateMenu(now);
            break;
        case Screen::About:
            updateAbout();
            break;
        case Screen::Game:
            updateGame(now);
            break;
    }

    tft.pushImage(0, 0, kScreenW, kScreenH, frameBuffer);
}
