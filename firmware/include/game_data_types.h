// Data model shared by the firmware and the asset generator.
//
// The PWA prototype (js/app.js) reads data/events.json, data/scenes.json and
// data/sprites.json at runtime. The firmware cannot afford a JSON parser plus
// the raw documents in RAM, so scripts/build_firmware_assets.py bakes the very
// same three files into the constant tables declared here and emits them into
// include/generated/*.h.
//
// Struct layout is the contract between generator and firmware: if a field is
// added/reordered here, regenerate the headers.
//
// Regenerate:  python scripts/build_firmware_assets.py

#pragma once

#include <cstdint>

namespace road_east::data {

// ---------------------------------------------------------------- sprites --
// One entry per sprite from data/sprites.json. Frames are laid out
// horizontally in the sheet, exactly like the source PNG.
struct SpriteDef {
    const char* name;
    int16_t frameWidth;
    int16_t frameHeight;
    int16_t frames;
    int16_t fps;
    int16_t sheetWidth;  // frameWidth * frames
    // RGB565, row-major, sheetWidth * frameHeight entries.
    const uint16_t* pixels;
    // 1bpp opacity mask, MSB-first, ((sheetWidth + 7) / 8) bytes per row.
    // Bit set = draw the pixel, clear = transparent.
    const uint8_t* mask;
};

// ----------------------------------------------------------------- scenes --
enum class LayerType : uint8_t {
    Rect = 0,
    Sprite = 1,
};

enum class SpriteAnchor : uint8_t {
    TopLeft = 0,
    CenterBottom = 1,
    LeftBottom = 2,
};

// Layer sprite index placeholder for scenes.json "${hero}" — resolved at draw
// time to the sprite of the player's current transport.
constexpr int8_t kSpriteHero = -1;

struct SceneLayer {
    LayerType type;
    // rect layers
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
    uint16_t color;
    // sprite layers
    int8_t sprite;  // index into sprites(), or kSpriteHero
    SpriteAnchor anchor;
    bool tile;           // repeat across the full scene width
    int16_t scrollSpeed; // px/sec, tiled layers scroll right-to-left
    int16_t spacing;     // extra gap between tiled copies
    bool animated;       // scenes.json animationSpeed != 0
    int8_t freezeFrame;  // >= 0 pins the frame, -1 animates normally
};

struct SceneDef {
    const char* id;
    uint16_t background;
    const SceneLayer* layers;
    uint8_t layerCount;
};

// ----------------------------------------------------------------- events --
// Scene reference sentinels. js/app.js distinguishes three cases for a
// choice's "scene" key: absent (inherit from the event / default scene),
// explicit null (play no transition at all) and a scene name.
constexpr int8_t kSceneInherit = -2;
constexpr int8_t kSceneNone = -1;

// "transport_mod" is absent on most choices; -1 means "leave as is".
constexpr int8_t kTransportKeep = -1;

struct ChoiceDef {
    const char* action;
    int8_t transportMod;
    int8_t foodMod;
    int8_t hpMod;
    int8_t moodMod;
    const char* nextId;
    int8_t scene;  // kSceneInherit / kSceneNone / index into scenes()
    // Result card, shown before the transition when any of these is set.
    const char* resultTitle;    // nullptr when absent
    const char* resultText;     // nullptr when absent
    int8_t resultScene;         // kSceneNone when absent
    const char* continueText;   // nullptr → firmware default prompt
};

struct EventDef {
    const char* id;
    const char* text;
    const char* hint;           // nullptr → firmware default
    int8_t scene;               // kSceneNone when absent
    const char* unlockFriend;   // party member id, nullptr when absent
    const char* unlockFriend2;
    int8_t fuelMod;
    const ChoiceDef* choices;
    uint8_t choiceCount;
};

}  // namespace road_east::data
