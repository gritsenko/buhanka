// Pixel-art scene renderer — the C++ twin of renderScene() in js/app.js.
//
// A scene is an ordered list of layers (data/scenes.json): `rect` becomes a
// Canvas::fillRect, `sprite` becomes a Canvas::blitSprite. Tiled layers scroll
// right-to-left at `scrollSpeed` px/sec, which is the parallax the prototype
// uses for mountains / trees / bushes.

#pragma once

#include "canvas.h"
#include "game_data_types.h"

namespace road_east::scene {

// Internal scene resolution, shared with the PWA (SCENE_W/SCENE_H in
// js/app.js). On the 240x135 panel it is drawn 1:1, centred.
constexpr int kSceneWidth = 204;
constexpr int kSceneHeight = 115;
constexpr int kSceneOriginX = (240 - kSceneWidth) / 2;
constexpr int kSceneOriginY = (135 - kSceneHeight) / 2;

// Draws `scene` at (originX, originY), clipped to the scene box. `timeSec` is
// the elapsed playback time and drives both frame animation and scrolling.
// `heroSpriteIndex` resolves the "${hero}" placeholder layers.
void render(gfx::Canvas& canvas, const data::SceneDef& scene, float timeSec,
            int heroSpriteIndex, int originX = kSceneOriginX,
            int originY = kSceneOriginY);

// Back-compat fallback from js/app.js buildFallbackScene(): a single centred
// hero on black, used when an event/choice names no scene at all.
const data::SceneDef& fallbackScene();

}  // namespace road_east::scene
