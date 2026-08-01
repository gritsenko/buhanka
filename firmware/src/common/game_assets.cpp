#include "game_assets.h"

#include <cstring>

namespace road_east::assets {

const data::SpriteDef* sprite(int index) {
    if (index < 0 || index >= kSpriteCount) return nullptr;
    return &kSprites[index];
}

int heroSpriteIndex(int transport) {
    if (transport < 0 || transport >= kTransportSpriteCount) transport = 0;
    return kTransportSprites[transport];
}

const data::SceneDef* scene(int index) {
    if (index < 0 || index >= kSceneCount) return nullptr;
    return &kScenes[index];
}

int defaultSceneIndex() { return kDefaultSceneIndex; }

const data::EventDef* event(int index) {
    if (index < 0 || index >= kEventCount) return nullptr;
    return &kEvents[index];
}

int eventIndexById(const char* id) {
    if (id == nullptr) return -1;
    for (int i = 0; i < kEventCount; ++i) {
        if (std::strcmp(kEvents[i].id, id) == 0) return i;
    }
    return -1;
}

const data::EventDef* eventById(const char* id) {
    return event(eventIndexById(id));
}

}  // namespace road_east::assets
