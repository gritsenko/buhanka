#include "scene_render.h"

#include <cmath>

#include "game_assets.h"

namespace road_east::scene {

namespace {

using data::SceneLayer;
using data::SpriteAnchor;

// Anchor → offset from the layer's (x, y) to the sprite's top-left corner.
// All frame sizes in data/sprites.json are even, so the halving is exact.
void anchorOffset(const data::SpriteDef& def, SpriteAnchor anchor, int& ax, int& ay) {
    switch (anchor) {
        case SpriteAnchor::CenterBottom:
            ax = -def.frameWidth / 2;
            ay = -def.frameHeight;
            break;
        case SpriteAnchor::LeftBottom:
            ax = 0;
            ay = -def.frameHeight;
            break;
        case SpriteAnchor::TopLeft:
        default:
            ax = 0;
            ay = 0;
            break;
    }
}

int pickFrame(const SceneLayer& layer, const data::SpriteDef& def, float timeSec) {
    if (layer.freezeFrame >= 0) {
        return layer.freezeFrame % def.frames;
    }
    if (def.frames > 1 && def.fps > 0) {
        // animationSpeed: 0 in scenes.json freezes the sheet on frame 0.
        const float frameTime = layer.animated ? timeSec : 0.0f;
        const int f = static_cast<int>(frameTime * static_cast<float>(def.fps));
        return ((f % def.frames) + def.frames) % def.frames;
    }
    return 0;
}

void drawSpriteLayer(gfx::Canvas& canvas, const SceneLayer& layer, float timeSec,
                     int heroSpriteIndex, int originX, int originY) {
    const int index = (layer.sprite == data::kSpriteHero) ? heroSpriteIndex : layer.sprite;
    const data::SpriteDef* def = assets::sprite(index);
    if (def == nullptr || def->frames <= 0) return;

    const int frame = pickFrame(layer, *def, timeSec);
    int ax = 0;
    int ay = 0;
    anchorOffset(*def, layer.anchor, ax, ay);

    if (layer.tile) {
        // Endless belt: one period is a frame plus its spacing; the belt is
        // shifted by scrollSpeed * time and wrapped into [0, period).
        const int period = def->frameWidth + layer.spacing;
        if (period <= 0) return;
        const int baseY = originY + layer.y + ay;
        float offset = std::fmod(static_cast<float>(layer.scrollSpeed) * timeSec,
                                 static_cast<float>(period));
        if (offset < 0.0f) offset += static_cast<float>(period);
        // Start one copy off-screen to the left so tiles slide in smoothly.
        // floor(x + 0.5), not lround: JS Math.round breaks ties towards +∞, so
        // the off-screen-left copy at exactly -2.5 lands on -2, not -3.
        for (float x = -offset; x < static_cast<float>(kSceneWidth); x += period) {
            const int px = static_cast<int>(std::floor(x + 0.5f));
            canvas.blitSprite(*def, frame, originX + px, baseY);
        }
        return;
    }

    canvas.blitSprite(*def, frame, originX + layer.x + ax, originY + layer.y + ay);
}

// buildFallbackScene() from js/app.js: the hero centred on a black backdrop.
constexpr SceneLayer kFallbackLayers[] = {
    {data::LayerType::Sprite, kSceneWidth / 2, kSceneHeight - 5, 0, 0, 0,
     data::kSpriteHero, SpriteAnchor::CenterBottom, false, 0, 0, true, -1},
};

constexpr data::SceneDef kFallbackScene{"", gfx::rgb565Hex(0x000000), kFallbackLayers, 1};

}  // namespace

const data::SceneDef& fallbackScene() { return kFallbackScene; }

void render(gfx::Canvas& canvas, const data::SceneDef& scene, float timeSec,
            int heroSpriteIndex, int originX, int originY) {
    canvas.setClip(originX, originY, kSceneWidth, kSceneHeight);
    canvas.fillRect(originX, originY, kSceneWidth, kSceneHeight, scene.background);

    for (int i = 0; i < scene.layerCount; ++i) {
        const SceneLayer& layer = scene.layers[i];
        if (layer.type == data::LayerType::Rect) {
            canvas.fillRect(originX + layer.x, originY + layer.y, layer.w, layer.h, layer.color);
        } else {
            drawSpriteLayer(canvas, layer, timeSec, heroSpriteIndex, originX, originY);
        }
    }

    canvas.resetClip();
}

}  // namespace road_east::scene
