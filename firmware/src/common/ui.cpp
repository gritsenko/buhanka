#include "ui.h"

#include <cstdio>

#include "game_assets.h"
#include "generated/splash_image.h"
#include "road_east_version.h"
#include "scene_render.h"
#include "text_render.h"

namespace road_east::ui {

namespace {

using gfx::Canvas;
using gfx::rgb565Hex;

// ---------------------------------------------------------------- palette --
// Lifted from the CSS custom properties in css/style.css; the translucent
// layers (rgba over --screen-bg) are pre-flattened.
constexpr uint16_t kColBlack = 0x0000;
constexpr uint16_t kColScreenBg = rgb565Hex(0x111411);
constexpr uint16_t kColTopBar = rgb565Hex(0x070807);
constexpr uint16_t kColDivider = rgb565Hex(0x222222);
constexpr uint16_t kColPartyBg = rgb565Hex(0x0C0E0C);
constexpr uint16_t kColCardBg = rgb565Hex(0x1B1E1B);
constexpr uint16_t kColCardBorder = rgb565Hex(0x3A3A3A);
constexpr uint16_t kColBarBg = rgb565Hex(0x333333);
constexpr uint16_t kColBarBgDim = rgb565Hex(0x1E1E1E);
constexpr uint16_t kColText = rgb565Hex(0xF0F0F0);
constexpr uint16_t kColTextDim = rgb565Hex(0x8A8A8A);
constexpr uint16_t kColMuted = rgb565Hex(0x5F5F5F);
constexpr uint16_t kColHp = rgb565Hex(0xFF4757);
constexpr uint16_t kColMood = rgb565Hex(0x1E90FF);
constexpr uint16_t kColFood = rgb565Hex(0xFFA502);
constexpr uint16_t kColFuel = rgb565Hex(0x9C88FF);
constexpr uint16_t kColAccent = rgb565Hex(0x3498DB);
constexpr uint16_t kColChoiceBg = rgb565Hex(0x202020);
constexpr uint16_t kColChoiceSel = rgb565Hex(0x30414F);
constexpr uint16_t kColChoiceEdge = rgb565Hex(0x3F3F3F);
constexpr uint16_t kColHeader = rgb565Hex(0xF1C40F);
constexpr uint16_t kColStatus = rgb565Hex(0xA5D6A7);
constexpr uint16_t kColHint = rgb565Hex(0xF39C12);
constexpr uint16_t kColMapText = rgb565Hex(0x81C784);
constexpr uint16_t kColMarker = rgb565Hex(0xFF4757);
constexpr uint16_t kColPanelBg = rgb565Hex(0x0A0F16);
constexpr uint16_t kColPanelEdge = rgb565Hex(0x3A5866);
constexpr uint16_t kColResultText = rgb565Hex(0xE8EDF2);
constexpr uint16_t kColContinue = rgb565Hex(0x8AD6FF);
constexpr uint16_t kColMenuBar = rgb565Hex(0x0B2545);
constexpr uint16_t kColMenuSel = rgb565Hex(0x1E4620);

// ----------------------------------------------------------------- layout --
constexpr int kTopBarH = 16;
constexpr int kPartyW = 60;
constexpr int kBarH = 8;
constexpr int kMemberCardH = 26;
constexpr int kMemberGap = 3;
constexpr int kChoiceRowH = 15;
constexpr int kChoiceGap = 3;
constexpr int kLineH = 12;  // == fontSmall().height

// ------------------------------------------------------------------ icons --
// 1bpp, MSB = leftmost pixel. The PWA uses emoji here; at 8px we draw them.
constexpr uint8_t kIconFood[8] = {0x18, 0x3C, 0x7E, 0x7E, 0x3C, 0x18, 0x10, 0x30};
// Jerrycan: a plain pump outline reads as a letter at this size.
constexpr uint8_t kIconFuel[8] = {0x30, 0x7E, 0x42, 0x5A, 0x5A, 0x42, 0x7E, 0x00};
constexpr uint8_t kIconPin[8] = {0x3C, 0x7E, 0xDB, 0xDB, 0xFF, 0x7E, 0x3C, 0x18};
constexpr uint8_t kIconHeart[6] = {0x6C, 0xFE, 0xFE, 0x7C, 0x38, 0x10};
constexpr uint8_t kIconFace[6] = {0x7C, 0x82, 0xAA, 0x82, 0xBA, 0x7C};

// ----------------------------------------------------------------- helpers --
int minInt(int a, int b) { return a < b ? a : b; }

// The CSS `blink` keyframes, sampled per frame.
bool blink(uint32_t nowMs, uint32_t periodMs) {
    return ((nowMs / (periodMs / 2)) % 2) == 0;
}

void fillTriangleRight(Canvas& canvas, int x, int y, int h, uint16_t color) {
    for (int i = 0; i < h; ++i) {
        const int half = (i <= h / 2) ? i : (h - 1 - i);
        canvas.drawHLine(x, y + i, half + 1, color);
    }
}

void fillTriangleUp(Canvas& canvas, int x, int y, int h, uint16_t color) {
    for (int i = 0; i < h; ++i) {
        canvas.drawHLine(x - i, y + i, i * 2 + 1, color);
    }
}

// Progress bar in the style of .bar-container / .bar-fill.
void drawBar(Canvas& canvas, int x, int y, int w, int h, int percent,
             uint16_t fill, bool dim) {
    canvas.fillRoundRect(x, y, w, h, h / 2, dim ? kColBarBgDim : kColBarBg);
    if (percent <= 0 || dim) return;
    if (percent > 100) percent = 100;
    const int inner = w - 2;
    const int filled = inner * percent / 100;
    if (filled > 0) {
        canvas.fillRoundRect(x + 1, y + 1, filled, h - 2, (h - 2) / 2, fill);
    }
}

void drawFlatBar(Canvas& canvas, int x, int y, int w, int h, int percent, uint16_t fill) {
    canvas.fillRect(x, y, w, h, kColBarBg);
    if (percent <= 0) return;
    if (percent > 100) percent = 100;
    canvas.fillRect(x, y, w * percent / 100, h, fill);
}

// Draw `utf8` clipped to a box; used everywhere a string could overrun.
void drawClipped(Canvas& canvas, const Font& font, int x, int y, int boxW,
                 const char* utf8, uint16_t color) {
    canvas.setClip(x, y, boxW, font.height);
    drawText(canvas, font, x, y, utf8, color);
    canvas.resetClip();
}

// 4bpp indexed splash from assets/sprites/splash.png.
void blitSplash(Canvas& canvas) {
    using namespace road_east::assets;
    for (int y = 0; y < kSplashHeight; ++y) {
        const uint8_t* src = &kSplashPixels[y * (kSplashWidth / 2)];
        for (int x = 0; x < kSplashWidth; x += 2) {
            const uint8_t byte = src[x / 2];
            canvas.drawPixel(x, y, kSplashPalette[byte >> 4]);
            canvas.drawPixel(x + 1, y, kSplashPalette[byte & 0x0F]);
        }
    }
}

// ------------------------------------------------------------- game screen --

void drawTopBar(Canvas& canvas, const game::Game& game) {
    canvas.fillRect(0, 0, kScreenWidth, kTopBarH, kColTopBar);
    canvas.drawHLine(0, kTopBarH - 1, kScreenWidth, kColDivider);

    canvas.blitMono(kIconFood, 8, 8, 3, 4, kColFood);
    drawBar(canvas, 13, 4, 88, kBarH, game.food(), kColFood, false);

    const bool noFuel = game.fuel() <= 0;
    canvas.blitMono(kIconFuel, 8, 8, 105, 4, noFuel ? kColMuted : kColFuel);
    drawBar(canvas, 115, 4, 88, kBarH, game.fuel(), kColFuel, noFuel);

    // .map-touch-btn — decorative here: the map lives on a long press.
    canvas.fillRoundRect(216, 2, 22, 12, 3, rgb565Hex(0x2A2A2C));
    canvas.drawRoundRect(216, 2, 22, 12, 3, rgb565Hex(0x444444));
    canvas.blitMono(kIconPin, 8, 8, 223, 4, kColMapText);
}

void drawPartyPanel(Canvas& canvas, const game::Game& game) {
    canvas.fillRect(0, kTopBarH, kPartyW, kScreenHeight - kTopBarH, kColPartyBg);
    canvas.drawVLine(kPartyW - 1, kTopBarH, kScreenHeight - kTopBarH, kColDivider);

    const Font& font = fontSmall();
    int slot = 0;
    for (int i = 0; i < game::kPartySize; ++i) {
        const game::PartyMember& m = game.member(i);
        if (!m.active) continue;

        const int y = kTopBarH + 3 + slot * (kMemberCardH + kMemberGap);
        ++slot;

        canvas.fillRoundRect(2, y, kPartyW - 4, kMemberCardH, 3, kColCardBg);
        canvas.drawRoundRect(2, y, kPartyW - 4, kMemberCardH, 3, kColCardBorder);

        drawClipped(canvas, font, 5, y + 1, kPartyW - 9, m.name, kColText);

        canvas.blitMono(kIconHeart, 7, 6, 5, y + 14, kColHp);
        drawFlatBar(canvas, 14, y + 15, 40, 4, m.hp, kColHp);

        canvas.blitMono(kIconFace, 7, 6, 5, y + 20, kColMood);
        drawFlatBar(canvas, 14, y + 21, 40, 4, m.mood, kColMood);
    }
}

void drawChoices(Canvas& canvas, const game::Game& game, int rowsTop) {
    const Font& font = fontSmall();
    const int n = game.choiceCount();
    constexpr int kRowX = 63;
    constexpr int kRowW = kScreenWidth - kRowX - 4;

    for (int i = 0; i < n; ++i) {
        const bool selected = (i == game.selectedChoice());
        const int y = rowsTop + i * (kChoiceRowH + kChoiceGap);

        canvas.fillRoundRect(kRowX, y, kRowW, kChoiceRowH, 3,
                             selected ? kColChoiceSel : kColChoiceBg);
        // .choice-row border-left
        canvas.fillRect(kRowX, y + 1, 3, kChoiceRowH - 2,
                        selected ? kColAccent : kColChoiceEdge);
        if (selected) {
            fillTriangleRight(canvas, kRowX + 7, y + 5, 5, kColAccent);
        }
        drawClipped(canvas, font, kRowX + 15, y + 1, kRowW - 19,
                    game.choice(i).action, selected ? kColText : kColTextDim);
    }
}

void drawContentPanel(Canvas& canvas, const game::Game& game) {
    const Font& font = fontSmall();
    const int n = game.choiceCount();
    const int rowsH = n > 0 ? n * kChoiceRowH + (n - 1) * kChoiceGap : 0;
    const int rowsTop = kScreenHeight - 3 - rowsH;

    const int textX = kPartyW + 5;
    const int textTop = kTopBarH + 4;
    const int textW = kScreenWidth - textX - 5;
    const int textCapacity = (rowsTop - 4 - textTop) / kLineH;

    TextLine lines[10];
    bool truncated = false;
    const int count = wrapText(font, game.currentEvent().text, textW, lines,
                               minInt(textCapacity, 10), &truncated);
    for (int i = 0; i < count; ++i) {
        drawTextN(canvas, font, textX, textTop + i * kLineH, lines[i].begin,
                  lines[i].bytes, kColText);
    }
    if (truncated && count > 0) {
        const int y = textTop + (count - 1) * kLineH;
        const int x = textX + textW - 3 * font.advance;
        canvas.fillRect(x, y, 3 * font.advance, font.height, kColScreenBg);
        drawText(canvas, font, x, y, "...", kColTextDim);
    }

    drawChoices(canvas, game, rowsTop);
}

// ---------------------------------------------------------------- overlays --

void drawStatusOverlay(Canvas& canvas, const game::Game& game) {
    const Font& font = fontSmall();
    char buf[40];

    canvas.clear(kColScreenBg);
    drawTextAnchored(canvas, font, kScreenWidth / 2, 1, "=== СОСТОЯНИЕ ГРУППЫ ===",
                     kColHeader, TextAnchor::TopCenter);
    canvas.drawHLine(4, 14, kScreenWidth - 8, kColDivider);

    // Left column — ЗАПАСЫ (generateStatusText() in js/app.js).
    int y = 18;
    drawText(canvas, font, 4, y, "ЗАПАСЫ:", kColText);
    y += kLineH - 1;
    std::snprintf(buf, sizeof buf, "Еда: %d%%", game.food());
    drawText(canvas, font, 4, y, buf, kColStatus);
    y += kLineH - 1;
    if (game.fuel() > 0) {
        std::snprintf(buf, sizeof buf, "Бензин: %d%%", game.fuel());
        drawText(canvas, font, 4, y, buf, kColStatus);
        y += kLineH - 1;
    }
    drawText(canvas, font, 4, y, "Транспорт:", kColStatus);
    y += kLineH - 1;
    drawClipped(canvas, font, 4, y, 112, game.transportName(), kColStatus);

    // Right column — ОТРЯД.
    constexpr int kRightX = 124;
    y = 18;
    drawText(canvas, font, kRightX, y, "ОТРЯД (ЗД/МР):", kColText);
    y += kLineH - 1;
    for (int i = 0; i < game::kPartySize; ++i) {
        const game::PartyMember& m = game.member(i);
        if (!m.active) continue;
        std::snprintf(buf, sizeof buf, "%s %d/%d", m.name, m.hp, m.mood);
        drawClipped(canvas, font, kRightX, y, kScreenWidth - kRightX - 4, buf, kColStatus);
        y += kLineH - 1;
    }

    // Hint, the .hint-text block of the PWA overlay. Four lines at 11px from
    // y=78 end at 121, clearing the footer at 123 — the longest hint in
    // data/events.json uses all four.
    constexpr int kHintTop = 78;
    constexpr int kHintLines = 4;
    constexpr int kFooterY = 123;
    static_assert(kHintTop + (kHintLines - 1) * (kLineH - 1) + kLineH <= kFooterY,
                  "status hint collides with the footer");

    canvas.drawDashedHLine(4, 74, kScreenWidth - 8, 3, 3, kColMuted);
    TextLine lines[kHintLines];
    const int count = wrapText(font, game.hint(), kScreenWidth - 8, lines, kHintLines);
    for (int i = 0; i < count; ++i) {
        drawTextN(canvas, font, 4, kHintTop + i * (kLineH - 1), lines[i].begin,
                  lines[i].bytes, kColHint);
    }

    drawTextAnchored(canvas, font, kScreenWidth / 2, kFooterY, "Удерж. ВЫБОР — в меню",
                     kColMuted, TextAnchor::TopCenter);
}

void drawMapOverlay(Canvas& canvas, uint32_t nowMs) {
    const Font& font = fontSmall();

    canvas.clear(kColScreenBg);
    drawTextAnchored(canvas, font, kScreenWidth / 2, 1, "=== КАРТА МАРШРУТА ===",
                     kColHeader, TextAnchor::TopCenter);
    canvas.drawHLine(4, 14, kScreenWidth - 8, kColDivider);

    // 37 glyphs at 6px = 222px, centred with a 9px margin on the 240px panel.
    constexpr const char* kRoute = "[МСК] -- [ОКА] -- [УРАЛ] .. [НАХОДКА]";
    constexpr int kRouteX = (kScreenWidth - 37 * 6) / 2;
    drawText(canvas, font, kRouteX, 46, kRoute, kColMapText);

    // .map-marker: blinking "▲ ВЫ ЗДЕСЬ" under the second waypoint.
    if (blink(nowMs, 1000)) {
        const int markerX = kRouteX + 11 * 6 + 3;
        fillTriangleUp(canvas, markerX, 60, 5, kColMarker);
        drawTextAnchored(canvas, font, markerX, 68, "ВЫ ЗДЕСЬ", kColMarker,
                         TextAnchor::TopCenter);
    }

    drawTextAnchored(canvas, font, kScreenWidth / 2, 118, "Любая кнопка — закрыть",
                     kColMuted, TextAnchor::TopCenter);
}

// -------------------------------------------------------- transition/result --

void drawScene(Canvas& canvas, const game::Game& game, uint32_t nowMs) {
    const data::SceneDef* s = game.activeScene();
    canvas.clear(kColBlack);
    if (s == nullptr) return;
    scene::render(canvas, *s, game.sceneTimeSec(nowMs), game.heroSpriteIndex());
}

void drawResult(Canvas& canvas, const game::Game& game, uint32_t nowMs) {
    const Font& font = fontSmall();
    drawScene(canvas, game, nowMs);

    // .result-card, floated over the (optional) scene.
    constexpr int kCardX = 8;
    constexpr int kCardW = kScreenWidth - 2 * kCardX;
    constexpr int kPad = 6;

    constexpr int kInnerW = kCardW - 2 * kPad;

    TextLine lines[5];
    bool truncated = false;
    const int count = wrapText(font, game.resultText(), kInnerW, lines, 5, &truncated);
    // The continue prompt is authored content too ("Нажмите, чтобы собраться и
    // идти дальше" is wider than the card), so wrap it instead of letting it
    // bleed past the edges.
    TextLine cont[2];
    const int contCount = wrapText(font, game.resultContinue(), kInnerW, cont, 2);

    const int cardH = kPad + kLineH + 3 + count * kLineH + kPad - 1 +
                      contCount * kLineH + kPad;
    const int cardY = (kScreenHeight - cardH) / 2;

    canvas.fillRoundRect(kCardX, cardY, kCardW, cardH, 4, kColPanelBg);
    canvas.drawRoundRect(kCardX, cardY, kCardW, cardH, 4, kColPanelEdge);

    drawTextAnchored(canvas, font, kScreenWidth / 2, cardY + kPad, game.resultTitle(),
                     kColHeader, TextAnchor::TopCenter);
    const int textY = cardY + kPad + kLineH + 3;
    for (int i = 0; i < count; ++i) {
        drawTextN(canvas, font, kCardX + kPad, textY + i * kLineH, lines[i].begin,
                  lines[i].bytes, kColResultText);
    }
    // Two result texts already fill all five lines, so a copy edit can overflow
    // the card. Mark it the same way the event panel does instead of cutting
    // the sentence off silently.
    if (truncated && count > 0) {
        const int y = textY + (count - 1) * kLineH;
        const int x = kCardX + kPad + kInnerW - 3 * font.advance;
        canvas.fillRect(x, y, 3 * font.advance, font.height, kColPanelBg);
        drawText(canvas, font, x, y, "...", kColResultText);
    }
    if (blink(nowMs, 1200)) {
        const int contY = cardY + cardH - kPad - contCount * kLineH;
        for (int i = 0; i < contCount; ++i) {
            drawTextAnchoredN(canvas, font, kScreenWidth / 2, contY + i * kLineH,
                              cont[i].begin, cont[i].bytes, kColContinue,
                              TextAnchor::TopCenter);
        }
    }
}

}  // namespace

// ------------------------------------------------------------------ public --

void drawSplash(Canvas& canvas, uint32_t nowMs) {
    blitSplash(canvas);
    if (!blink(nowMs, 1000)) return;
    const int promptY = kScreenHeight - 18;
    canvas.fillRect(0, promptY - 2, kScreenWidth, 18, kColBlack);
    drawTextAnchored(canvas, fontLarge(), kScreenWidth / 2, promptY, "Нажмите кнопку",
                     kColHeader, TextAnchor::TopCenter);
}

void drawMenu(Canvas& canvas, int cursor, bool canContinue) {
    static const char* const kItems[3] = {"Новая поездка", "Продолжить", "Об игре"};

    canvas.clear(kColScreenBg);
    canvas.fillRect(0, 0, kScreenWidth, 22, kColMenuBar);
    drawTextAnchored(canvas, fontLarge(), kScreenWidth / 2, 3, "ДОРОГА НА ВОСТОК",
                     kColText, TextAnchor::TopCenter);

    constexpr int kItemTop = 36;
    constexpr int kItemH = 24;
    for (int i = 0; i < 3; ++i) {
        const bool selected = (i == cursor);
        const bool enabled = (i != 1) || canContinue;
        const int y = kItemTop + i * kItemH;
        if (selected) {
            canvas.fillRoundRect(20, y - 3, kScreenWidth - 40, 22, 4, kColMenuSel);
        }
        const uint16_t fg = !enabled ? kColMuted : (selected ? kColText : kColTextDim);
        drawTextAnchored(canvas, fontLarge(), kScreenWidth / 2, y, kItems[i], fg,
                         TextAnchor::TopCenter);
    }

    drawTextAnchored(canvas, fontSmall(), kScreenWidth / 2, kScreenHeight - 14,
                     "Верх: выбор    Низ: далее", kColMuted, TextAnchor::TopCenter);
}

void drawAbout(Canvas& canvas) {
    const Font& font = fontSmall();

    canvas.clear(kColScreenBg);
    canvas.fillRect(0, 0, kScreenWidth, 18, kColMenuBar);
    drawTextAnchored(canvas, font, kScreenWidth / 2, 3, "ОБ ИГРЕ", kColHeader,
                     TextAnchor::TopCenter);

    // 9 lines at 11px starting at y=20 end at 119, clearing the version
    // footer at 122. Adding a line here overlaps it — check the arithmetic.
    static const char* const kLines[] = {
        "Постапокалиптический дорожный",
        "квест: выбраться из Подмосковья",
        "на восток, к Находке. Следите за",
        "едой, здоровьем и настроением",
        "отряда — и ищите транспорт.",
        "",
        "ВЕРХ: выбор пункта / статус",
        "НИЗ:  подтвердить / карта",
        "(второе — долгим нажатием)",
    };
    constexpr int kLineCount = sizeof(kLines) / sizeof(kLines[0]);
    static_assert(20 + (kLineCount - 1) * (kLineH - 1) + kLineH <= kScreenHeight - 15,
                  "About text collides with the version footer");
    for (int i = 0; i < kLineCount; ++i) {
        drawText(canvas, font, 4, 20 + i * (kLineH - 1), kLines[i], kColStatus);
    }

    char buf[48];
    std::snprintf(buf, sizeof buf, "%s %s", road_east::kFirmwareName,
                  road_east::kFirmwareVersion);
    drawTextAnchored(canvas, font, kScreenWidth / 2, kScreenHeight - 13, buf, kColMuted,
                     TextAnchor::TopCenter);
}

void drawGame(Canvas& canvas, const game::Game& game, uint32_t nowMs) {
    switch (game.phase()) {
        case game::Phase::Transition:
            drawScene(canvas, game, nowMs);
            return;
        case game::Phase::Result:
            drawResult(canvas, game, nowMs);
            return;
        case game::Phase::Event:
            break;
    }

    switch (game.overlay()) {
        case game::Overlay::Status:
            drawStatusOverlay(canvas, game);
            return;
        case game::Overlay::Map:
            drawMapOverlay(canvas, nowMs);
            return;
        case game::Overlay::None:
            break;
    }

    canvas.clear(kColScreenBg);
    drawTopBar(canvas, game);
    drawPartyPanel(canvas, game);
    drawContentPanel(canvas, game);
}

}  // namespace road_east::ui
