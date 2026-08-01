// Game state machine — a straight port of js/app.js.
//
// Everything here is deliberately allocation-free and device-independent: the
// hardware client feeds it button edges plus millis(), and ui.cpp renders it.

#pragma once

#include <cstdint>

#include "game_data_types.h"

namespace road_east::game {

constexpr int kPartySize = 4;

// Fixed transition length, TRANSITION_MS in js/app.js.
constexpr uint32_t kTransitionMs = 1500;

struct PartyMember {
    const char* id;
    const char* name;
    int hp;
    int mood;
    bool active;
};

enum class Phase : uint8_t {
    Event,       // reading the event text, picking a choice
    Transition,  // pixel-art scene plays for kTransitionMs, then the next event
    Result,      // consequence card, waits for a button press
};

enum class Overlay : uint8_t {
    None,
    Status,  // party status + hint (long press on the top button)
    Map,     // route map (long press on the bottom button)
};

class Game {
public:
    // Full restart: resets stats and loads the "start" event.
    void reset(uint32_t nowMs);

    // Drives the transition timer. Call once per frame.
    void tick(uint32_t nowMs);

    // ----- input -----
    void selectNext();               // top button, short press
    void confirm(uint32_t nowMs);    // bottom button, short press
    void openStatus();               // top button, long press
    void openMap();                  // bottom button, long press
    bool closeOverlay();             // true when an overlay was actually closed

    // While a transition plays, js/app.js ignores every button.
    bool busy() const { return phase_ == Phase::Transition; }

    // ----- view state -----
    Phase phase() const { return phase_; }
    Overlay overlay() const { return overlay_; }

    int food() const { return food_; }
    int fuel() const { return fuel_; }
    int transport() const { return transport_; }
    const char* transportName() const;
    int heroSpriteIndex() const;

    const PartyMember& member(int index) const { return party_[index]; }
    int activeMemberCount() const;

    const data::EventDef& currentEvent() const;
    int choiceCount() const;
    const data::ChoiceDef& choice(int index) const;
    int selectedChoice() const { return selected_; }
    const char* hint() const;

    // Scene to draw for the current phase, or nullptr when there is none.
    const data::SceneDef* activeScene() const;
    float sceneTimeSec(uint32_t nowMs) const;

    const char* resultTitle() const { return resultTitle_; }
    const char* resultText() const { return resultText_; }
    const char* resultContinue() const;

private:
    void resetStats();
    void loadEvent(const char* eventId);
    void advanceToEvent(const char* eventId, uint32_t nowMs);
    void applyChoice(uint32_t nowMs);
    void showResult(const data::ChoiceDef& choice, uint32_t nowMs);
    void continueResult(uint32_t nowMs);
    void playTransition(const data::ChoiceDef* choice, const char* nextId, uint32_t nowMs);
    const data::SceneDef* pickScene(const data::ChoiceDef* choice) const;
    void unlockFriend(const char* memberId);
    void clampStats();

    int food_ = 100;
    int fuel_ = 0;
    int transport_ = 0;  // 0 — on foot, 1 — bike, 2 — UAZ
    PartyMember party_[kPartySize] = {
        {"hero", "Я", 100, 100, true},
        {"sashka", "Сашка", 100, 100, false},
        {"ilya", "Илья", 100, 100, false},
        {"maks", "Макс", 100, 100, false},
    };

    int eventIndex_ = 0;
    int selected_ = 0;
    Phase phase_ = Phase::Event;
    Overlay overlay_ = Overlay::None;

    // Transition / result bookkeeping.
    uint32_t sceneStartedAt_ = 0;
    const data::SceneDef* activeScene_ = nullptr;
    const char* pendingNextId_ = nullptr;
    const data::ChoiceDef* pendingChoice_ = nullptr;
    const char* resultTitle_ = nullptr;
    const char* resultText_ = nullptr;
    const char* resultContinue_ = nullptr;
};

}  // namespace road_east::game
