// Screen composition for "Дорога на восток".
//
// Mirrors the PWA layout (index.html + css/style.css) inside 240x135:
// a resource top bar, the party panel on the left, event text with the choice
// list on the right, plus the status / map / transition / result overlays.

#pragma once

#include <cstdint>

#include "canvas.h"
#include "game.h"

namespace road_east::ui {

constexpr int kScreenWidth = 240;
constexpr int kScreenHeight = 135;

// Boot artwork with a blinking prompt.
void drawSplash(gfx::Canvas& canvas, uint32_t nowMs);

// Main menu. `canContinue` greys out "Продолжить" until a run has been started.
void drawMenu(gfx::Canvas& canvas, int cursor, bool canContinue);

void drawAbout(gfx::Canvas& canvas);

// The game itself: picks the right layout for the current phase and overlay.
void drawGame(gfx::Canvas& canvas, const game::Game& game, uint32_t nowMs);

}  // namespace road_east::ui
