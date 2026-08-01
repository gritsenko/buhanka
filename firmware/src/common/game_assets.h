// Convenience accessors over the tables baked in by
// scripts/build_firmware_assets.py.

#pragma once

#include "game_data_types.h"
#include "generated/events_data.h"
#include "generated/scenes_data.h"
#include "generated/sprites_data.h"

namespace road_east::assets {

const data::SpriteDef* sprite(int index);
// Sprite for a transport level (0 — on foot, 1 — bike, 2 — UAZ), i.e. the
// "${hero}" placeholder in data/scenes.json.
int heroSpriteIndex(int transport);

const data::SceneDef* scene(int index);
int defaultSceneIndex();

const data::EventDef* event(int index);
int eventIndexById(const char* id);
const data::EventDef* eventById(const char* id);

}  // namespace road_east::assets
