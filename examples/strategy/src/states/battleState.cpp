#include "battleState.h"

#include <stormengine2/components/animation.h>
#include <stormengine2/components/sprite.h>
#include <stormengine2/components/transform.h>
#include <stormengine2/systems/animation.h>
#include <stormengine2/systems/render.h>

#include <algorithm>
#include <cmath>

#include "../ui.h"

const std::string BattleState::s_battleID = "BATTLE";

namespace {

constexpr float kSoldierScale = 0.55f;   // 192 * 0.55 ~= 106px
constexpr int   kRowCount     = 2;

// Frame counts, measured from the Tiny Swords free pack. Getting one of these
// wrong does not fail loudly -- AnimationSystem just walks off the end of the
// strip and draws empty space -- so they are named rather than inlined.
// Wrap-safe "now is at or past deadline". `now >= deadline` on two Uint32s
// inverts across the ~49-day SDL_GetTicks wrap; the signed difference does not.
bool Expired(Uint32 now, Uint32 deadline) {
    return static_cast<Sint32>(now - deadline) >= 0;
}

int FrameCount(world::Troop troop, const char *action) {
    const bool archer = (troop == world::Troop::Archer);
    if (std::string(action) == "idle")   return archer ? 6 : 8;
    if (std::string(action) == "run")    return archer ? 4 : 6;
    if (std::string(action) == "attack") return archer ? 8 : 4;
    return 1;
}

} // namespace

BattleState::BattleState(SDL_Renderer *renderer, int windowWidth,
                         int windowHeight, bool isDebugging,
                         AssetStore *assetStore, GameStateMachine *machine,
                         Gamepad *gamepad, world::Campaign *campaign,
                         int attacker, int defender, int castle,
                         bool &isRunning)
    : renderer_{renderer}, windowWidth_{windowWidth},
      windowHeight_{windowHeight}, isDebugging_{isDebugging},
      assetStore_{assetStore}, machine_{machine}, gamepad_{gamepad},
      campaign_{campaign}, isRunning_{isRunning}, castle_{castle} {
    const auto &ga = campaign_->generals[attacker];
    const auto &gd = campaign_->generals[defender];

    attacker_ = {ga.side, ga.troop, attacker, ga.troops, ga.troops,
                 Command::Hold, 0, {}};
    defender_ = {gd.side, gd.troop, defender, gd.troops, gd.troops,
                 Command::Hold, 0, {}};

    attackerOnLeft_ = (attacker_.side == world::Owner::Blue);
}

std::string BattleState::UnitAsset(const Side &s, const char *action) const {
    const std::string colour = (s.side == world::Owner::Blue) ? "blue" : "red";
    // Spearman has no art of its own in the free pack -- see world.h -- so it
    // borrows the Warrior's. The counter table is the mechanic, not the sprite.
    const std::string kind =
        (s.troop == world::Troop::Archer) ? "archer" : "warrior";
    return colour + "_" + kind + "_" + action;
}

bool BattleState::onEnter() {
    registry_.AddSystem<RenderSystem>();
    registry_.AddSystem<AnimationSystem>();

    SpawnSoldiers(attacker_, attackerOnLeft_);
    SpawnSoldiers(defender_, !attackerOnLeft_);

    // Deferred creation again: without this flush the first frame of the battle
    // shows an empty field.
    registry_.Update();

    lastTicks_ = SDL_GetTicks();
    logger_.Log("BattleState entered: " +
                std::string(world::TroopName(attacker_.troop)) + " " +
                std::to_string(attacker_.troops) + " vs " +
                world::TroopName(defender_.troop) + " " +
                std::to_string(defender_.troops));
    return true;
}

bool BattleState::onExit() {
    logger_.Log("BattleState exited");
    return true;
}

void BattleState::SpawnSoldiers(Side &s, bool facingRight) {
    const int drawn = static_cast<int>(kSoldierScale * UNIT_FRAME);
    const int baseY = windowHeight_ / 2 - drawn / 2;

    for (int i = 0; i < VISIBLE_PER_SIDE; ++i) {
        const int row = i % kRowCount;
        const int col = i / kRowCount;

        // Ranks start at their own edge and close toward the middle. The x here
        // is the "open" position; update() lerps them inward as closeT_ rises.
        const int spread = 74;
        const int x = facingRight ? (40 + col * spread)
                                  : (windowWidth_ - 40 - drawn - col * spread);
        const int y = baseY + (row - (kRowCount - 1) / 2) * 96 - 40;

        Entity e = registry_.CreateEntity();
        e.AddComponent<TransformComponent>(
            glm::vec2(x, y), glm::vec2(kSoldierScale, kSoldierScale), 0.0);
        e.AddComponent<SpriteComponent>(UnitAsset(s, "run"), UNIT_FRAME,
                                        UNIT_FRAME, /*zIndex=*/row + 1, false,
                                        0, 0);
        e.AddComponent<AnimationComponent>(FrameCount(s.troop, "run"), 10,
                                           /*vertical=*/false,
                                           /*isLooped=*/true);

        // Tiny Swords sprites face right. The side coming from the right edge
        // is the same art mirrored -- there is no second set to load.
        if (!facingRight) {
            if (auto *sprite = e.TryGetComponent<SpriteComponent>()) {
                sprite->flip = SDL_FLIP_HORIZONTAL;
            }
        }
        s.soldiers.push_back(e);
    }
}

void BattleState::SetAnimation(Side &s, const char *action, int frames,
                               bool looped) {
    const std::string asset = UnitAsset(s, action);
    for (auto &e : s.soldiers) {
        auto *sprite = e.TryGetComponent<SpriteComponent>();
        auto *anim   = e.TryGetComponent<AnimationComponent>();
        if (!sprite || !anim) {
            continue;
        }
        if (sprite->assetId == asset) {
            continue;   // already playing; restarting would stutter
        }
        sprite->assetId     = asset;
        sprite->srcRect.x   = 0;
        anim->numFrames     = frames;
        anim->currentFrame  = 1;
        anim->isLooped      = looped;
        anim->startTime     = SDL_GetTicks();
    }
}

// Thins the rank as the troop count falls, so eight sprites a side read as an
// army being ground down.
//
// Casualties are KILLED, not hidden. An earlier version pushed their zIndex to
// -100 on the theory that it would tuck them behind the background: it does not.
// RenderSystem sorts by zIndex and then draws every entity it holds, and the
// battle's background is an SDL_RenderClear plus a band rect issued *before* the
// registry renders -- there is no backdrop entity to hide behind, so a -100
// sprite simply drew first and stayed fully visible. Both armies showed all
// eight men no matter how few troops were left.
//
// Killing is deferred to the next Registry::Update(), so the entity is still in
// the render system for the remainder of this frame. That is fine: the count
// only ever falls, so nothing needs it back.
void BattleState::SyncSoldierCount(Side &s) {
    const int alive =
        (s.startTroops <= 0)
            ? 0
            : static_cast<int>(std::ceil(VISIBLE_PER_SIDE *
                                         (static_cast<float>(s.troops) /
                                          s.startTroops)));

    while (static_cast<int>(s.soldiers.size()) > alive) {
        s.soldiers.back().Kill();
        s.soldiers.pop_back();
    }
}

void BattleState::SpawnHitEffect(glm::vec2 pos) {
    // By value, not by reference: callers pass a soldier's own
    // TransformComponent::position, and AddComponent below resizes that same
    // pool before reading the argument.
    Entity e = registry_.CreateEntity();
    e.AddComponent<TransformComponent>(pos, glm::vec2(0.7f, 0.7f), 0.0);
    e.AddComponent<SpriteComponent>("explosion", 192, 192, /*zIndex=*/50, false,
                                    0, 0);
    e.AddComponent<AnimationComponent>(8, 14, /*vertical=*/false,
                                       /*isLooped=*/false);
    effects_.push_back(e);
}

// AnimationSystem stops a non-looping animation on its last frame and leaves
// the entity alone -- there is no "animation finished" callback and no
// self-destruct flag. Anything one-shot has to notice for itself.
void BattleState::CullFinishedEffects() {
    auto done = [](Entity &e) {
        const auto *anim = e.TryGetComponent<AnimationComponent>();
        if (!anim) {
            return true;   // component gone: nothing left to wait for
        }
        // `numFrames - 1`, NOT `numFrames`. AnimationSystem clamps a
        // non-looping animation with
        //     currentFrame = min(max(frame, 0), min(last, numFrames - 1))
        // so currentFrame can never reach numFrames, and the obvious test never
        // fires at all: the effects then live for the whole battle and the
        // entity count only climbs.
        return anim->currentFrame >= anim->numFrames - 1;
    };

    for (auto &e : effects_) {
        if (done(e)) {
            e.Kill();
        }
    }
    effects_.erase(std::remove_if(effects_.begin(), effects_.end(), done),
                   effects_.end());
}

// The player is always Blue, whether Blue marched in or is holding the castle.
// Orders therefore have to follow the colour, not the attacker/defender role --
// routing them to attacker_ meant that in every battle the player defended, the
// command bar was driving the enemy: pressing CHARGE applied the 1.6x multiplier
// to the army killing them.
BattleState::Side &BattleState::PlayerSide() {
    return attacker_.side == world::Owner::Blue ? attacker_ : defender_;
}

BattleState::Side &BattleState::AiSide() {
    return attacker_.side == world::Owner::Blue ? defender_ : attacker_;
}

void BattleState::IssueCommand(Command c) {
    if (over_) {
        return;
    }
    if (c == Command::Retreat) {
        // Ask first. Every other order is a few seconds of modifier and wears
        // off; this one ends the battle and hands over the castle, so a
        // mispress is unrecoverable.
        confirmRetreat_ = true;
        pausedAtMs_     = SDL_GetTicks();
        return;
    }
    Side &player        = PlayerSide();
    player.command      = c;
    player.commandUntil = SDL_GetTicks() + COMMAND_MS;
}

void BattleState::ConfirmRetreat() {
    if (!confirmRetreat_) {
        return;
    }
    confirmRetreat_ = false;
    lastTicks_      = SDL_GetTicks();   // the prompt swallowed real time
    logger_.Log("Retreat confirmed");
    // The player's side quits the field, so the other side wins -- which is not
    // the same as "the defender wins". When the player is defending, retreating
    // hands the castle to the attacker.
    Finish(AiSide().side);
}

void BattleState::CancelRetreat() {
    if (!confirmRetreat_) {
        return;
    }
    confirmRetreat_ = false;

    // Give back the time the prompt was up. Both deadlines are absolute
    // SDL_GetTicks values, so without this a long deliberation would silently
    // expire the standing order and, once the battle is over, cut short the
    // outcome banner -- or skip it entirely and pop straight back to the map.
    const Uint32 paused = SDL_GetTicks() - pausedAtMs_;
    attacker_.commandUntil += paused;
    defender_.commandUntil += paused;
    overAtMs_              += paused;
    lastTicks_ = SDL_GetTicks();
    logger_.Log("Retreat cancelled");
}

void BattleState::ResolveTick() {
    if (over_) {
        return;
    }

    Side &player = PlayerSide();
    Side &ai     = AiSide();

    if (Expired(SDL_GetTicks(), player.commandUntil)) {
        player.command = Command::Hold;
    }

    // The AI picks its own order so the fight is not one-sided: charge while
    // ahead, hold while behind.
    ai.command = (ai.troops >= player.troops) ? Command::Charge : Command::Hold;

    auto damage = [&](const Side &from, const Side &to) {
        float d = from.troops * 0.06f * world::Counter(from.troop, to.troop);
        switch (from.command) {
        case Command::Charge: d *= 1.6f; break;
        case Command::Hold:   d *= 0.7f; break;
        case Command::Volley:
            // Archers only. Anyone else shouting "volley" is just holding.
            d *= (from.troop == world::Troop::Archer) ? 1.8f : 0.7f;
            break;
        case Command::Retreat: d = 0.0f; break;
        }
        // Holding also reduces damage taken, which is what makes it a real
        // choice rather than a strictly worse charge. A non-Archer VOLLEY is
        // treated as a hold on both sides of the ledger: it already takes the
        // hold penalty to its own output above, and reading it as anything else
        // here would make it strictly worse than the order it is standing in for.
        const bool braced =
            to.command == Command::Hold ||
            (to.command == Command::Volley && to.troop != world::Troop::Archer);
        if (braced) {
            d *= 0.65f;
        }
        return static_cast<int>(std::ceil(d));
    };

    const int toDefender = damage(attacker_, defender_);
    const int toAttacker = damage(defender_, attacker_);

    defender_.troops = std::max(0, defender_.troops - toDefender);
    attacker_.troops = std::max(0, attacker_.troops - toAttacker);

    SyncSoldierCount(attacker_);
    SyncSoldierCount(defender_);

    // One effect per tick per side, at the inner edge of each line, so the clash
    // has a pulse without spawning an entity per casualty.
    for (Side *s : {&attacker_, &defender_}) {
        if (s->soldiers.empty()) {
            continue;
        }
        if (auto *t = s->soldiers.front().TryGetComponent<TransformComponent>()) {
            const float towardCentre =
                (t->position.x < windowWidth_ / 2.0f) ? 60.0f : -60.0f;
            SpawnHitEffect(t->position + glm::vec2(towardCentre, -20.0f));
        }
    }

    if (defender_.troops <= 0 && attacker_.troops <= 0) {
        // Mutual annihilation goes to the defender: the attack failed.
        Finish(defender_.side);
    } else if (defender_.troops <= 0) {
        Finish(attacker_.side);
    } else if (attacker_.troops <= 0) {
        Finish(defender_.side);
    }
}

void BattleState::Finish(world::Owner winner) {
    if (over_) {
        return;
    }
    over_     = true;
    winner_   = winner;
    overAtMs_ = SDL_GetTicks();

    auto &result        = campaign_->result;
    result.pending      = true;
    result.attacker     = attacker_.general;
    result.defender     = defender_.general;
    result.castle       = castle_;
    result.winner       = winner;
    // The floor of 1 is load-bearing, not cosmetic. On mutual annihilation both
    // counts are 0 and the surviving side would be recorded with no troops --
    // resume() writes this straight back onto the general, and a general with 0
    // troops deals `ceil(0 * ...)` = 0 damage forever, so its next battle could
    // never end. A last survivor holding the field is the cheapest rule that
    // keeps every general able to fight.
    result.winnerTroops = std::max(
        1, (winner == attacker_.side) ? attacker_.troops : defender_.troops);

    logger_.Log(std::string("Battle over, winner ") +
                (winner == world::Owner::Blue ? "Blue" : "Red"));
}

void BattleState::processInput() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        gamepad_->HandleEvent(event);

        if (event.type == SDL_QUIT) {
            isRunning_ = false;
            return;
        }
        if (event.type != SDL_KEYDOWN || event.key.repeat) {
            continue;
        }

        // Modal. While the retreat prompt is up it is the only thing listening,
        // so a stray order key cannot slip through and a mis-aimed press cannot
        // confirm by accident.
        if (confirmRetreat_) {
            switch (event.key.keysym.sym) {
            case SDLK_RETURN:
            case SDLK_KP_ENTER:
            case SDLK_y:
                ConfirmRetreat();
                return;
            // ESC cancels the prompt rather than quitting the game. That is what
            // a modal is expected to do, and it keeps ESC from ever being the
            // key that loses a castle.
            case SDLK_ESCAPE:
            case SDLK_n:
            case SDLK_4:
                CancelRetreat();
                break;
            default:
                break;
            }
            continue;
        }

        switch (event.key.keysym.sym) {
        case SDLK_1: IssueCommand(Command::Charge);  break;
        case SDLK_2: IssueCommand(Command::Hold);    break;
        case SDLK_3: IssueCommand(Command::Volley);  break;
        case SDLK_4: IssueCommand(Command::Retreat); break;
        case SDLK_ESCAPE:
            isRunning_ = false;
            return;
        default:
            break;
        }
    }
}

void BattleState::update() {
    gamepad_->Update();

    // Retreat is on Y, not Back. Back quits the game in every other state, so
    // binding it to a move that concedes a castle meant a player who had learnt
    // "Back leaves" surrendered instead -- and left no way to quit from here at
    // all. All four orders now sit on the face buttons, in the same order as the
    // on-screen bar, and Back means the same thing everywhere.
    if (confirmRetreat_) {
        // Modal on the pad too: A confirms, B cancels, everything else is
        // ignored. Y deliberately does NOT confirm -- Y is what opened the
        // prompt, and a double-tap would defeat the point of asking.
        if (gamepad_->PressedA() || gamepad_->PressedStart()) {
            ConfirmRetreat();
        } else if (gamepad_->PressedB() || gamepad_->PressedBack()) {
            CancelRetreat();
        }

        // Damage stops, but the scene does not. Animations keep running and
        // finished effects keep being culled, so the pause reads as a held
        // breath rather than as the game having hung -- and the effect entities
        // do not sit around waiting for the player to make up their mind.
        CullFinishedEffects();
        registry_.Update();
        registry_.GetSystem<AnimationSystem>().Update();
        return;
    }

    if (gamepad_->PressedA()) IssueCommand(Command::Charge);
    if (gamepad_->PressedB()) IssueCommand(Command::Hold);
    if (gamepad_->PressedX()) IssueCommand(Command::Volley);
    if (gamepad_->PressedY()) IssueCommand(Command::Retreat);
    if (gamepad_->PressedBack()) {
        isRunning_ = false;
        return;
    }

    const Uint32 now   = SDL_GetTicks();
    const Uint32 delta = now - lastTicks_;
    lastTicks_         = now;

    // Close the gap first, then fight. Damage does not start until the lines
    // touch, so the approach reads as part of the battle rather than dead time.
    if (!closed_) {
        closeT_ += delta / 900.0f;
        if (closeT_ >= 1.0f) {
            closeT_ = 1.0f;
            closed_ = true;
            SetAnimation(attacker_, "attack",
                         FrameCount(attacker_.troop, "attack"), true);
            SetAnimation(defender_, "attack",
                         FrameCount(defender_.troop, "attack"), true);
        }

        const int drawn  = static_cast<int>(kSoldierScale * UNIT_FRAME);
        const int centre = windowWidth_ / 2;
        auto close = [&](Side &s, bool fromLeft) {
            for (int i = 0; i < static_cast<int>(s.soldiers.size()); ++i) {
                auto *t = s.soldiers[i].TryGetComponent<TransformComponent>();
                if (!t) continue;
                const int col    = i / kRowCount;
                const int spread = 74;
                const float open = fromLeft ? (40 + col * spread)
                                            : (windowWidth_ - 40 - drawn -
                                               col * spread);
                const float shut = fromLeft
                                       ? (centre - drawn - 10 - col * 56)
                                       : (centre + 10 + col * 56);
                t->position.x = open + (shut - open) * closeT_;
            }
        };
        close(attacker_, attackerOnLeft_);
        close(defender_, !attackerOnLeft_);
    } else if (!over_) {
        tickAccumMs_ += delta;
        while (tickAccumMs_ >= TICK_MS && !over_) {
            tickAccumMs_ -= TICK_MS;
            ResolveTick();
        }
    }

    CullFinishedEffects();

    registry_.Update();
    registry_.GetSystem<AnimationSystem>().Update();

    if (over_ && !popped_ && SDL_GetTicks() - overAtMs_ > OUTCOME_MS) {
        popped_ = true;

        // Popping the LAST state leaves the machine empty, and an empty machine
        // is not an exit: Game::Run keeps looping on isRunning_, but
        // processInput/update/render all return immediately because there is no
        // state to forward to. Nothing then calls SDL_PollEvent, so the window
        // stops responding, the process spins at 100% CPU, and it cannot even
        // be closed -- SIGTERM is delivered but never turned into an SDL_QUIT.
        //
        // That only happens via `./bin/realms battle`, which opens directly on
        // this state with nothing beneath it. In the campaign the overworld is
        // always underneath.
        if (machine_->getGameStates().size() <= 1) {
            isRunning_ = false;
            return;
        }

        // Pop, do not change: the overworld is still underneath with all of its
        // state intact, and its resume() applies the result written in Finish().
        machine_->popState();
        return;
    }
}

void BattleState::RenderHud() {
    SDL_Texture *digits = assetStore_->GetTexture("digits");

    auto sideBar = [&](const Side &s, bool left) {
        const int w = 300;
        const int x = left ? 20 : windowWidth_ - w - 20;
        const SDL_Color colour = (s.side == world::Owner::Blue)
                                     ? ui::BlueColour()
                                     : ui::RedColour();
        ui::DrawPanel(renderer_, x, 18, w, 74);

        SDL_SetRenderDrawColor(renderer_, colour.r, colour.g, colour.b, 255);
        SDL_Rect swatch{x + 14, 32, 22, 22};
        SDL_RenderFillRect(renderer_, &swatch);

        ui::DrawNumber(renderer_, digits, s.troops, x + 48, 28, 0.85f);
        ui::DrawBar(renderer_, x + 14, 62, w - 28, 22,
                    s.startTroops ? static_cast<float>(s.troops) / s.startTroops
                                  : 0.0f,
                    colour);
    };
    sideBar(attacker_, attackerOnLeft_);
    sideBar(defender_, !attackerOnLeft_);

    // Command bar. The active order is boxed.
    const char *ids[4] = {"cmd_charge", "cmd_hold", "cmd_volley", "cmd_retreat"};
    const int   n      = 4;
    const int   slotW  = 210;
    const int   startX = windowWidth_ / 2 - (n * slotW) / 2;
    const int   y      = windowHeight_ - 84;

    // The player's own order is the one to box, which is not attacker_ whenever
    // the player is the one defending.
    const Side &player = (attacker_.side == world::Owner::Blue) ? attacker_
                                                                : defender_;
    for (int i = 0; i < n; ++i) {
        SDL_Texture *tex = assetStore_->GetTexture(ids[i]);
        const int cx = startX + i * slotW + slotW / 2;
        const bool active = !over_ &&
                            static_cast<int>(player.command) == i &&
                            !Expired(SDL_GetTicks(), player.commandUntil);
        if (active) {
            ui::DrawPanel(renderer_, cx - slotW / 2 + 8, y - 10, slotW - 16, 52,
                          200);
        }
        ui::DrawTextureCentred(renderer_, tex, cx, y, 0.95f);
    }

    ui::DrawTextureCentred(renderer_, assetStore_->GetTexture("hint_battle"),
                           windowWidth_ / 2, windowHeight_ - 26, 0.9f);

    if (over_) {
        // The player is always Blue, so "victory" is Blue's win.
        SDL_Texture *banner = assetStore_->GetTexture(
            winner_ == world::Owner::Blue ? "victory" : "defeat");
        ui::DrawTextureCentred(renderer_, banner, windowWidth_ / 2,
                               windowHeight_ / 2 - 170, 1.0f);
    }
}

// Drawn over the battle, not instead of it: the player is being asked about the
// fight, so the fight has to stay on screen behind the question.
void BattleState::RenderRetreatPrompt() {
    // Dim everything first, which is what makes the panel read as modal rather
    // than as one more HUD element.
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer_, 12, 8, 20, 170);
    SDL_Rect full{0, 0, windowWidth_, windowHeight_};
    SDL_RenderFillRect(renderer_, &full);

    // Deliberately NOT centred. The ranks sit across the middle of the screen,
    // and a centred panel covers exactly the thing the player is being asked
    // about -- how the fight is actually going. This sits above them, under the
    // strength bars.
    const int w = 560;
    const int h = 184;
    const int x = windowWidth_ / 2 - w / 2;
    const int y = 100;
    ui::DrawPanel(renderer_, x, y, w, h, 235);

    const int cx = windowWidth_ / 2;
    ui::DrawTextureCentred(renderer_, assetStore_->GetTexture("confirm_retreat"),
                           cx, y + 22, 1.0f);
    ui::DrawTextureCentred(renderer_, assetStore_->GetTexture("confirm_body"),
                           cx, y + 86, 1.0f);
    ui::DrawTextureCentred(renderer_, assetStore_->GetTexture("confirm_hint"),
                           cx, y + 136, 1.0f);
}

void BattleState::render() {
    // A flat field rather than a tilemap: the battle is a different register
    // from the map, and GameStateMachine::render() draws only the top state, so
    // nothing of the overworld shows through anyway.
    SDL_SetRenderDrawColor(renderer_, 96, 132, 74, 255);
    SDL_RenderClear(renderer_);

    SDL_SetRenderDrawColor(renderer_, 84, 116, 66, 255);
    SDL_Rect band{0, windowHeight_ / 2 - 40, windowWidth_, 220};
    SDL_RenderFillRect(renderer_, &band);

    registry_.GetSystem<RenderSystem>().Update(renderer_, *assetStore_);
    RenderHud();

    if (confirmRetreat_) {
        RenderRetreatPrompt();
    }

    SDL_RenderPresent(renderer_);
}
