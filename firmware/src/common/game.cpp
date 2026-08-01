#include "game.h"

#include <cstring>

#include "game_assets.h"
#include "scene_render.h"

namespace road_east::game {

namespace {

int clampStat(int v) { return v < 0 ? 0 : (v > 100 ? 100 : v); }

bool present(const char* s) { return s != nullptr && s[0] != '\0'; }

// choiceHasResult() in js/app.js is truthiness-based, so an empty result_text
// shows no card — match that rather than treating "" as set.
bool choiceHasResult(const data::ChoiceDef& c) {
    return present(c.resultText) || present(c.resultTitle) ||
           c.resultScene != data::kSceneNone;
}

constexpr const char* kDefaultHint = "Подсказки нет.";
constexpr const char* kDefaultContinue = "Нажмите, чтобы продолжить";

}  // namespace

// ------------------------------------------------------------- lifecycle --

void Game::resetStats() {
    food_ = 100;
    fuel_ = 0;
    transport_ = 0;
    for (int i = 0; i < kPartySize; ++i) {
        party_[i].hp = 100;
        party_[i].mood = 100;
        party_[i].active = (std::strcmp(party_[i].id, "hero") == 0);
    }
}

void Game::reset(uint32_t nowMs) {
    resetStats();
    phase_ = Phase::Event;
    overlay_ = Overlay::None;
    pendingChoice_ = nullptr;
    pendingNextId_ = nullptr;
    activeScene_ = nullptr;
    sceneStartedAt_ = nowMs;
    loadEvent("start");
}

void Game::clampStats() {
    food_ = clampStat(food_);
    fuel_ = clampStat(fuel_);
    for (int i = 0; i < kPartySize; ++i) {
        if (!party_[i].active) continue;
        party_[i].hp = clampStat(party_[i].hp);
        party_[i].mood = clampStat(party_[i].mood);
    }
}

void Game::unlockFriend(const char* memberId) {
    if (memberId == nullptr) return;
    for (int i = 0; i < kPartySize; ++i) {
        if (std::strcmp(party_[i].id, memberId) == 0) {
            party_[i].active = true;
            return;
        }
    }
}

void Game::loadEvent(const char* eventId) {
    // Death check, exactly as in loadEvent() in js/app.js: it runs before the
    // lookup, so a fatal choice reroutes to game_over whatever it pointed at.
    if ((food_ <= 0 || party_[0].hp <= 0) && std::strcmp(eventId, "start") != 0) {
        eventId = "game_over";
    }

    const int index = assets::eventIndexById(eventId);
    if (index < 0) {
        // The generator rejects unknown next_id, so this is unreachable with
        // baked content — but returning while phase_ is still Transition would
        // wedge the game with input permanently blocked. Stay on the current
        // event instead, which is what the PWA does.
        phase_ = Phase::Event;
        return;
    }

    eventIndex_ = index;
    const data::EventDef& ev = *assets::event(index);

    unlockFriend(ev.unlockFriend);
    unlockFriend(ev.unlockFriend2);
    fuel_ += ev.fuelMod;

    selected_ = 0;
    phase_ = Phase::Event;
    activeScene_ = nullptr;
    pendingChoice_ = nullptr;
    pendingNextId_ = nullptr;
    clampStats();
}

void Game::advanceToEvent(const char* eventId, uint32_t nowMs) {
    (void)nowMs;
    if (eventId == nullptr) return;
    if (std::strcmp(eventId, "start") == 0) resetStats();
    loadEvent(eventId);
}

void Game::tick(uint32_t nowMs) {
    if (phase_ != Phase::Transition) return;
    if (nowMs - sceneStartedAt_ < kTransitionMs) return;
    advanceToEvent(pendingNextId_, nowMs);
}

// ------------------------------------------------------------ transitions --

const data::SceneDef* Game::pickScene(const data::ChoiceDef* choice) const {
    // pickScene() in js/app.js: a choice may override the scene, explicitly
    // suppress it (null), or leave it to the event / default scene.
    if (choice != nullptr && choice->scene != data::kSceneInherit) {
        if (choice->scene == data::kSceneNone) return nullptr;
        const data::SceneDef* s = assets::scene(choice->scene);
        return s != nullptr ? s : &scene::fallbackScene();
    }

    int index = currentEvent().scene;
    if (index == data::kSceneNone) index = assets::defaultSceneIndex();
    const data::SceneDef* s = assets::scene(index);
    return s != nullptr ? s : &scene::fallbackScene();
}

void Game::playTransition(const data::ChoiceDef* choice, const char* nextId, uint32_t nowMs) {
    const data::SceneDef* s = pickScene(choice);
    if (s == nullptr) {
        advanceToEvent(nextId, nowMs);
        return;
    }
    activeScene_ = s;
    pendingNextId_ = nextId;
    pendingChoice_ = nullptr;
    phase_ = Phase::Transition;
    sceneStartedAt_ = nowMs;
}

void Game::showResult(const data::ChoiceDef& choice, uint32_t nowMs) {
    pendingChoice_ = &choice;
    pendingNextId_ = choice.nextId;
    resultTitle_ = choice.resultTitle != nullptr ? choice.resultTitle : "Последствия выбора";
    resultText_ = choice.resultText != nullptr ? choice.resultText : "";
    resultContinue_ = choice.continueText;
    activeScene_ = assets::scene(choice.resultScene);  // nullptr when kSceneNone
    phase_ = Phase::Result;
    sceneStartedAt_ = nowMs;
}

void Game::continueResult(uint32_t nowMs) {
    const data::ChoiceDef* choice = pendingChoice_;
    const char* nextId = pendingNextId_;
    pendingChoice_ = nullptr;
    activeScene_ = nullptr;
    playTransition(choice, nextId, nowMs);
}

void Game::applyChoice(uint32_t nowMs) {
    if (phase_ == Phase::Result) {
        continueResult(nowMs);
        return;
    }
    if (phase_ != Phase::Event) return;
    if (choiceCount() == 0) return;

    const data::ChoiceDef& c = choice(selected_);

    if (c.transportMod != data::kTransportKeep) transport_ = c.transportMod;
    food_ += c.foodMod;
    for (int i = 0; i < kPartySize; ++i) {
        if (!party_[i].active) continue;
        party_[i].hp += c.hpMod;
        party_[i].mood += c.moodMod;
    }
    clampStats();

    if (choiceHasResult(c)) {
        showResult(c, nowMs);
        return;
    }
    playTransition(&c, c.nextId, nowMs);
}

// ------------------------------------------------------------------ input --

bool Game::closeOverlay() {
    if (overlay_ == Overlay::None) return false;
    overlay_ = Overlay::None;
    return true;
}

void Game::selectNext() {
    if (closeOverlay()) return;
    if (phase_ != Phase::Event) return;
    const int n = choiceCount();
    if (n > 1) selected_ = (selected_ + 1) % n;
}

void Game::confirm(uint32_t nowMs) {
    if (closeOverlay()) return;
    applyChoice(nowMs);
}

void Game::openStatus() {
    if (closeOverlay()) return;
    overlay_ = Overlay::Status;
}

void Game::openMap() {
    if (closeOverlay()) return;
    overlay_ = Overlay::Map;
}

// ------------------------------------------------------------------- view --

const data::EventDef& Game::currentEvent() const { return *assets::event(eventIndex_); }

int Game::choiceCount() const { return currentEvent().choiceCount; }

const data::ChoiceDef& Game::choice(int index) const {
    const data::EventDef& ev = currentEvent();
    if (index < 0 || index >= ev.choiceCount) index = 0;
    return ev.choices[index];
}

const char* Game::hint() const {
    const char* h = currentEvent().hint;
    return h != nullptr ? h : kDefaultHint;
}

const char* Game::resultContinue() const {
    return resultContinue_ != nullptr ? resultContinue_ : kDefaultContinue;
}

const char* Game::transportName() const {
    switch (transport_) {
        case 0: return "Пешком";
        case 1: return "Велосипед";
        default: return "УАЗ Буханка";
    }
}

int Game::heroSpriteIndex() const { return assets::heroSpriteIndex(transport_); }

int Game::activeMemberCount() const {
    int n = 0;
    for (int i = 0; i < kPartySize; ++i) {
        if (party_[i].active) ++n;
    }
    return n;
}

const data::SceneDef* Game::activeScene() const {
    if (phase_ == Phase::Transition || phase_ == Phase::Result) return activeScene_;
    return nullptr;
}

float Game::sceneTimeSec(uint32_t nowMs) const {
    return static_cast<float>(nowMs - sceneStartedAt_) / 1000.0f;
}

}  // namespace road_east::game
