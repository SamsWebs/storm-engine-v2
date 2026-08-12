#include "overworldState.h"

#include <stormengine2/components/animation.h>
#include <stormengine2/components/sprite.h>
#include <stormengine2/components/transform.h>
#include <stormengine2/systems/animation.h>
#include <stormengine2/systems/render.h>
#include <stormengine2/tilemapLoader.h>

#include <algorithm>
#include <cmath>

#include "../ui.h"
#include "battleState.h"
#include "gameOverState.h"

const std::string OverworldState::s_overworldID = "OVERWORLD";

namespace {

// Castle.png is 320x256; drawn at a third it sits inside a couple of tiles
// without swamping the map.
constexpr float kCastleScale = 0.34f;
constexpr int   kCastleW     = 320;
constexpr int   kCastleH     = 256;

// Army markers reuse the unit art at a small scale. Frame is 192x192.
constexpr float kArmyScale = 0.30f;
constexpr int   kUnitFrame = 192;

// Draw order. Explicit constants rather than literals scattered through the
// spawners: RenderSystem sorts on these, and two things sharing a zIndex sort
// against each other unpredictably (the sort is not stable), so every layer
// that can overlap another needs its own number.
enum ZIndex {
    kZWater  = 0,
    kZGround = 1,
    kZScrub  = 2,   // rocks and bushes, flat on the ground
    kZTree   = 3,   // tall, but still under anything the player must see
    kZCastle = 4,
    kZArmy   = 5,
};

// Decoration keep-out. Nothing is placed within kClearCastle of a castle centre
// or kClearRoad of a road, so scenery never sits under a castle sprite or under
// a marching army -- the marker lerps straight between castle centres, so the
// roads are exactly those segments.
constexpr float kClearCastle = 116.0f;
constexpr float kClearRoad   = 46.0f;
constexpr float kClearDeco   = 92.0f;   // scenery to scenery

// Distance from point p to segment ab. Used for the road keep-out.
float DistanceToSegment(glm::vec2 p, glm::vec2 a, glm::vec2 b) {
    const glm::vec2 ab = b - a;
    const float len2 = ab.x * ab.x + ab.y * ab.y;
    if (len2 <= 0.0001f) {
        return glm::length(p - a);
    }
    float t = ((p.x - a.x) * ab.x + (p.y - a.y) * ab.y) / len2;
    t = std::min(std::max(t, 0.0f), 1.0f);
    return glm::length(p - (a + ab * t));
}

const char *CastleAsset(world::Owner o) {
    switch (o) {
    case world::Owner::Blue: return "castle_blue";
    case world::Owner::Red:  return "castle_red";
    default:                 return "castle_neutral";
    }
}

// Marching armies show the running animation of their troop type. Spearman has
// no art of its own -- see the note in world.h -- so it borrows the Warrior's.
std::string RunAsset(world::Owner side, world::Troop troop) {
    const std::string colour = (side == world::Owner::Blue) ? "blue" : "red";
    const std::string kind = (troop == world::Troop::Archer) ? "archer"
                                                             : "warrior";
    return colour + "_" + kind + "_run";
}

int RunFrames(world::Troop troop) {
    // Warrior_Run.png is 1152x192 (6 frames); Archer_Run.png is 768x192 (4).
    return (troop == world::Troop::Archer) ? 4 : 6;
}

} // namespace

OverworldState::OverworldState(SDL_Renderer *renderer, int windowWidth,
                               int windowHeight, bool isDebugging,
                               AssetStore *assetStore, GameStateMachine *machine,
                               Gamepad *gamepad, world::Campaign *campaign,
                               bool &isRunning)
    : renderer_{renderer}, windowWidth_{windowWidth},
      windowHeight_{windowHeight}, isDebugging_{isDebugging},
      assetStore_{assetStore}, machine_{machine}, gamepad_{gamepad},
      campaign_{campaign}, isRunning_{isRunning} {}

bool OverworldState::onEnter() {
    // Systems are registered BEFORE any entity is created. AddSystem<T>() never
    // scans entities that already exist -- membership is computed once, when
    // the entity is flushed -- so a system added later would sit empty forever.
    registry_.AddSystem<RenderSystem>();
    registry_.AddSystem<AnimationSystem>();

    SpawnTerrain();
    SpawnDecorations();
    SpawnCastles();

    // Entity creation is deferred until Registry::Update(). Without this flush
    // the first frame after the changeState renders an empty map.
    registry_.Update();

    SyncArmyMarkers();
    registry_.Update();

    CycleSelection(0);
    lastTicks_ = SDL_GetTicks();
    logger_.Log("OverworldState entered");
    return true;
}

bool OverworldState::onExit() {
    logger_.Log("OverworldState exited");
    return true;
}

void OverworldState::SpawnTerrain() {
    TileMapLoader loader{"./assets/maps/overworld.map",
                         "./assets/gfx/terrain/tilemap.png", TILE};
    const auto &map = loader.getMap();
    if (map.empty()) {
        // getMap() is empty when the file was missing, unreadable, or parsed to
        // nothing. Every one of those is already reported by the loader, so say
        // what it means for the game rather than repeating the cause.
        logger_.Err("Overworld map is empty -- the campaign will have no ground.");
        return;
    }

    // Water sits under the transparent tiles rather than being a tile itself,
    // so the map only has to describe land.
    for (int y = 0; y < MAP_H; ++y) {
        for (int x = 0; x < MAP_W; ++x) {
            Entity water = registry_.CreateEntity();
            water.AddComponent<TransformComponent>(
                glm::vec2(x * TILE, y * TILE), glm::vec2(1.0f, 1.0f), 0.0);
            water.AddComponent<SpriteComponent>("water", 64, 64, kZWater,
                                                false, 0, 0);
        }
    }

    for (const auto &tile : map) {
        Entity e = registry_.CreateEntity();
        e.AddComponent<TransformComponent>(
            glm::vec2(tile.relativePosition.x * TILE,
                      tile.relativePosition.y * TILE),
            glm::vec2(1.0f, 1.0f), 0.0);
        e.AddComponent<SpriteComponent>("tilemap", TILE, TILE, kZGround,
                                        false, tile.pixelSrcPosition.x,
                                        tile.pixelSrcPosition.y);
    }
}

// Scatters trees, bushes and rocks over the open ground.
//
// Deterministic on purpose. The placement comes from a fixed-seed LCG rather
// than rand(), so the map looks the same on every run and in every screenshot;
// a decoration that wanders between runs makes any visual regression
// impossible to spot. It is also seeded here rather than authored as a table so
// that moving a castle re-flows the scenery around it instead of leaving a tree
// standing in the courtyard.
void OverworldState::SpawnDecorations() {
    struct Kind {
        const char *asset;
        int   frame;     // source frame size, square
        int   frames;    // 1 = static
        float scale;
        int   zIndex;
    };
    // Tree3.png is 1536x192 (8 frames of 192), Bushe1.png is 1024x128 (8 frames
    // of 128), Rock1.png is a single 64x64 tile.
    static const Kind kKinds[] = {
        {"tree", 192, 8, 0.52f, kZTree},
        {"bush", 128, 8, 0.55f, kZScrub},
        {"rock",  64, 1, 0.85f, kZScrub},
    };

    // Castle centres, and the road segments armies actually travel.
    glm::vec2 centres[world::kCastleCount];
    for (int i = 0; i < world::kCastleCount; ++i) {
        centres[i] = CastlePixel(i) + glm::vec2(kCastleW * kCastleScale / 2.0f,
                                                kCastleH * kCastleScale / 2.0f);
    }

    // Numerical Recipes' LCG constants; any decent pair would do. The seed is
    // arbitrary but fixed.
    Uint32 rng = 0x5eed1234u;
    auto next = [&rng]() {
        rng = rng * 1664525u + 1013904223u;
        return rng >> 16;   // high bits: the low ones cycle far too regularly
    };

    int placed = 0;
    int trees  = 0;
    std::vector<glm::vec2> taken;

    // Bounded attempts, not "loop until placed": with a full keep-out set there
    // may simply be no room left, and a while-until-N loop would spin forever.
    for (int attempt = 0; attempt < 400 && placed < DECO_COUNT; ++attempt) {
        // Inside the coastline: the outer ring of land carries the shoreline
        // tiles, and a tree half-off the beach reads as a mistake.
        const int tx = 2 + static_cast<int>(next() % (MAP_W - 4));
        const int ty = 2 + static_cast<int>(next() % (MAP_H - 4));
        const glm::vec2 centre((tx + 0.5f) * TILE, (ty + 0.5f) * TILE);

        bool blocked = false;
        for (int i = 0; i < world::kCastleCount && !blocked; ++i) {
            if (glm::length(centre - centres[i]) < kClearCastle) {
                blocked = true;
            }
            for (int j = i + 1; j < world::kCastleCount && !blocked; ++j) {
                if (campaign_->Adjacent(i, j) &&
                    DistanceToSegment(centre, centres[i], centres[j]) <
                        kClearRoad) {
                    blocked = true;
                }
            }
        }
        // Keep scenery off other scenery. The keep-outs above leave only the
        // band between the castle rows genuinely open, so without a spacing
        // rule everything lands in the middle and reads as an orchard rather
        // than as countryside.
        for (const auto &t : taken) {
            if (glm::length(centre - t) < kClearDeco) {
                blocked = true;
                break;
            }
        }
        if (blocked) {
            continue;
        }

        // Trees are the loudest thing on an otherwise flat map, so they are
        // capped rather than left to the dice; past the cap this rolls scrub
        // only. Index 0 is the tree, 1 and 2 are the bush and the rock.
        const int pick = (trees >= MAX_TREES) ? 1 + static_cast<int>(next() % 2)
                                              : static_cast<int>(next() % 3);
        const Kind &k = kKinds[pick];
        if (pick == 0) {
            ++trees;
        }
        const float drawn = k.frame * k.scale;

        Entity e = registry_.CreateEntity();
        e.AddComponent<TransformComponent>(
            centre - glm::vec2(drawn / 2.0f), glm::vec2(k.scale, k.scale), 0.0);
        e.AddComponent<SpriteComponent>(k.asset, k.frame, k.frame, k.zIndex,
                                        false, 0, 0);
        if (k.frames > 1) {
            e.AddComponent<AnimationComponent>(k.frames, 6, /*vertical=*/false,
                                               /*isLooped=*/true);
            // Stagger the phase. AnimationComponent stamps startTime in its
            // constructor, so everything spawned in this one frame would
            // otherwise sway in perfect unison, which reads as a glitch rather
            // than as wind.
            if (auto *anim = e.TryGetComponent<AnimationComponent>()) {
                anim->startTime -= next() % 2000;
            }
        }
        taken.push_back(centre);
        ++placed;
    }

    logger_.Log("Scattered " + std::to_string(placed) + " decorations (" +
                std::to_string(trees) + " trees)");
}

glm::vec2 OverworldState::CastlePixel(int castle) const {
    const auto &c = campaign_->castles[castle];
    // Centre the sprite on its tile.
    return glm::vec2(c.tile.x * TILE - (kCastleW * kCastleScale - TILE) / 2.0f,
                     c.tile.y * TILE - (kCastleH * kCastleScale - TILE) / 2.0f);
}

void OverworldState::SpawnCastles() {
    for (int i = 0; i < world::kCastleCount; ++i) {
        Entity e = registry_.CreateEntity();
        e.AddComponent<TransformComponent>(
            CastlePixel(i), glm::vec2(kCastleScale, kCastleScale), 0.0);
        e.AddComponent<SpriteComponent>(CastleAsset(campaign_->castles[i].owner),
                                        kCastleW, kCastleH, kZCastle, false,
                                        0, 0);
        castleEntities_[i] = e;
    }
}

void OverworldState::RefreshCastleSprite(int castle) {
    if (!castleEntities_[castle].has_value()) {
        return;
    }
    // TryGetComponent, not GetComponent: the latter hands back a shared fallback
    // on a miss, so a typo'd lookup would silently write into an object nothing
    // renders.
    if (auto *sprite =
            castleEntities_[castle]->TryGetComponent<SpriteComponent>()) {
        sprite->assetId = CastleAsset(campaign_->castles[castle].owner);
    }
}

glm::vec2 OverworldState::GeneralPixel(int general) const {
    const auto &g = campaign_->generals[general];

    // Interpolate between the castles' CENTRES, then offset for the marker's own
    // size. Lerping the castles' draw origins instead put the marker a castle's
    // width off its road, because CastlePixel is pre-shifted to centre a
    // 320x256 sprite at 0.34 and the marker is a 192x192 sprite at 0.30.
    auto centreOf = [&](int castle) {
        return CastlePixel(castle) + glm::vec2(kCastleW * kCastleScale / 2.0f,
                                               kCastleH * kCastleScale / 2.0f);
    };
    const glm::vec2 half(kUnitFrame * kArmyScale / 2.0f);

    const glm::vec2 from = centreOf(g.atCastle);
    if (g.toCastle < 0) {
        return from - half;
    }
    const glm::vec2 to = centreOf(g.toCastle);
    const float t = 1.0f - (g.marchDays / static_cast<float>(MARCH_DAYS));
    return from + (to - from) * std::min(std::max(t, 0.0f), 1.0f) - half;
}

void OverworldState::SyncArmyMarkers() {
    for (int i = 0; i < world::kGeneralCount; ++i) {
        const auto &g = campaign_->generals[i];
        const bool wants = g.alive && g.toCastle >= 0;

        if (!wants) {
            if (armyEntities_[i].has_value()) {
                armyEntities_[i]->Kill();
                armyEntities_[i].reset();
            }
            continue;
        }

        if (!armyEntities_[i].has_value()) {
            Entity e = registry_.CreateEntity();
            e.AddComponent<TransformComponent>(
                GeneralPixel(i), glm::vec2(kArmyScale, kArmyScale), 0.0);
            e.AddComponent<SpriteComponent>(RunAsset(g.side, g.troop),
                                            kUnitFrame, kUnitFrame,
                                            kZArmy, false, 0, 0);
            e.AddComponent<AnimationComponent>(RunFrames(g.troop), 10,
                                               /*vertical=*/false,
                                               /*isLooped=*/true);
            armyEntities_[i] = e;
            continue;
        }

        if (auto *t = armyEntities_[i]->TryGetComponent<TransformComponent>()) {
            t->position = GeneralPixel(i);
        }
    }
}

void OverworldState::RebuildDestinations() {
    destinations_.clear();
    if (selected_ < 0) {
        return;
    }
    const int from = campaign_->generals[selected_].atCastle;
    for (int c = 0; c < world::kCastleCount; ++c) {
        if (campaign_->Adjacent(from, c)) {
            destinations_.push_back(c);
        }
    }
    if (destSlot_ >= static_cast<int>(destinations_.size())) {
        destSlot_ = 0;
    }
}

void OverworldState::CycleSelection(int delta) {
    // Only idle, living Blue generals can be given an order.
    std::vector<int> pickable;
    for (int i = 0; i < world::kGeneralCount; ++i) {
        const auto &g = campaign_->generals[i];
        if (g.alive && g.side == world::Owner::Blue && g.toCastle < 0) {
            pickable.push_back(i);
        }
    }
    if (pickable.empty()) {
        selected_ = -1;
        destinations_.clear();
        return;
    }

    auto it = std::find(pickable.begin(), pickable.end(), selected_);
    int at = (it == pickable.end()) ? 0
                                    : static_cast<int>(it - pickable.begin());
    const int n = static_cast<int>(pickable.size());
    at = ((at + delta) % n + n) % n;
    selected_ = pickable[at];
    destSlot_ = 0;
    RebuildDestinations();
}

void OverworldState::CycleDestination(int delta) {
    if (destinations_.empty()) {
        return;
    }
    const int n = static_cast<int>(destinations_.size());
    destSlot_ = ((destSlot_ + delta) % n + n) % n;
}

void OverworldState::StartMarch(int general, int destination) {
    auto &g = campaign_->generals[general];
    if (!g.alive || g.toCastle >= 0 || !campaign_->Adjacent(g.atCastle, destination)) {
        return;
    }
    g.toCastle  = destination;
    g.marchDays = static_cast<float>(MARCH_DAYS);
    logger_.Log("General " + std::to_string(general) + " marches to castle " +
                std::to_string(destination));
}

void OverworldState::RunEnemyAI() {
    for (int i = 0; i < world::kGeneralCount; ++i) {
        auto &g = campaign_->generals[i];
        if (!g.alive || g.side != world::Owner::Red || g.toCastle >= 0) {
            continue;
        }
        // Two passes, and the second one matters more than it looks.
        //
        // Pass 1 takes an adjacent castle that is not already Red and is not
        // already another Red general's target -- without the second test the
        // whole enemy side piles onto one castle and the map stops moving.
        //
        // Pass 2 is the fallback: if every neighbour is already Red, the general
        // repositions through friendly territory instead of standing still. With
        // pass 1 alone a general enclosed by its own castles was inert for the
        // rest of the game, and once every surviving Red general was enclosed
        // the campaign could not progress at all -- no marches, no battles, so
        // nothing could ever end it.
        auto claimed = [&](int c) {
            for (int j = 0; j < world::kGeneralCount; ++j) {
                if (j != i && campaign_->generals[j].alive &&
                    campaign_->generals[j].side == world::Owner::Red &&
                    campaign_->generals[j].toCastle == c) {
                    return true;
                }
            }
            return false;
        };

        int best = -1;
        for (int c = 0; c < world::kCastleCount && best < 0; ++c) {
            if (!campaign_->Adjacent(g.atCastle, c)) continue;
            if (campaign_->castles[c].owner == world::Owner::Red) continue;
            if (claimed(c)) continue;
            best = c;
        }

        if (best < 0) {
            // Reposition. Skipping castles another general already garrisons
            // keeps the side spread out rather than stacking on one tile.
            for (int c = 0; c < world::kCastleCount && best < 0; ++c) {
                if (!campaign_->Adjacent(g.atCastle, c)) continue;
                if (claimed(c)) continue;
                if (campaign_->GarrisonAt(c) >= 0) continue;
                best = c;
            }
        }

        if (best >= 0) {
            StartMarch(i, best);
        }
    }
}

void OverworldState::AdvanceDay() {
    campaign_->day++;

    for (auto &g : campaign_->generals) {
        if (g.alive && g.toCastle >= 0) {
            g.marchDays -= 1.0f;
        }
    }
    RunEnemyAI();
}

void OverworldState::CheckArrivals() {
    if (awaitingBattle_ || leaving_) {
        return;
    }

    for (int i = 0; i < world::kGeneralCount; ++i) {
        auto &g = campaign_->generals[i];
        if (!g.alive || g.toCastle < 0 || g.marchDays > 0.0f) {
            continue;
        }

        const int dest     = g.toCastle;
        const int garrison = campaign_->GarrisonAt(dest);

        // The two questions are separate, and conflating them was a latent bug:
        // an earlier `contested` ORed "castle is not mine" with "garrison is an
        // enemy", then treated any garrison at a contested castle as a defender.
        // A friendly general standing in a castle our side does not own would
        // therefore have been fought as if it were the enemy -- and resume()
        // resolves by comparing Owner values, so it would have credited the
        // castle to one of our own generals and destroyed the other.
        const bool enemyCastle = campaign_->castles[dest].owner != g.side;
        const bool enemyGarrison =
            garrison >= 0 && campaign_->generals[garrison].side != g.side;

        if (!enemyCastle && !enemyGarrison) {
            // Friendly move: no fight, just arrive.
            g.atCastle = dest;
            g.toCastle = -1;
            // A general that just became idle is newly orderable. Without this
            // the order panel stays blank -- selected_ is only recomputed on an
            // arrow press -- so a player whose generals were all marching had no
            // visible way to know control had come back.
            if (g.side == world::Owner::Blue && selected_ < 0) {
                CycleSelection(0);
            }
            continue;
        }

        if (!enemyGarrison) {
            // Undefended (or friendly-held) enemy castle changes hands without
            // a battle.
            g.atCastle                     = dest;
            g.toCastle                     = -1;
            campaign_->castles[dest].owner = g.side;
            RefreshCastleSprite(dest);
            logger_.Log("Castle " + std::to_string(dest) + " taken unopposed");
            if (g.side == world::Owner::Blue && selected_ < 0) {
                CycleSelection(0);
            }
            continue;
        }
        const int defender = garrison;

        // A real fight. Push the battle ON TOP of this state: everything here
        // -- the registry, the entities, the day counter -- stays alive
        // underneath, and resume() picks it back up.
        awaitingBattle_        = true;
        campaign_->result      = world::BattleResult{};
        machine_->pushState(new BattleState(
            renderer_, windowWidth_, windowHeight_, isDebugging_, assetStore_,
            machine_, gamepad_, campaign_, i, defender, dest, isRunning_));
        return;
    }
}

bool OverworldState::CampaignOver() const {
    return campaign_->CountOwned(world::Owner::Blue) == world::kCastleCount ||
           campaign_->CountOwned(world::Owner::Red) == world::kCastleCount;
}

// Called after EVERY path that can change castle ownership, not just after a
// battle.
//
// This used to live only in resume(), which fires only when a battle pops. But
// ownership also flips on the no-battle path in CheckArrivals, and that is how
// a campaign usually finishes: once one side has lost its last general its
// castles are ungarrisoned, so the other side walks into each of them
// unopposed. The sixth capture then satisfied the win condition with nothing
// left to observe it, and the map became an absorbing state -- RunEnemyAI skips
// castles it already owns, so no general ever marched again, no battle could be
// pushed, and resume() could never fire. The result was a live window with a
// running day counter, dead input, and no way out but ESC.
bool OverworldState::MaybeEndCampaign() {
    if (leaving_ || !CampaignOver()) {
        return false;
    }
    leaving_ = true;
    const world::Owner winner =
        campaign_->CountOwned(world::Owner::Blue) == world::kCastleCount
            ? world::Owner::Blue
            : world::Owner::Red;
    logger_.Log(std::string("Campaign over, winner ") +
                (winner == world::Owner::Blue ? "Blue" : "Red"));
    machine_->changeState(new GameOverState(
        renderer_, windowWidth_, windowHeight_, isDebugging_, assetStore_,
        machine_, gamepad_, campaign_, winner, isRunning_));
    return true;
}

void OverworldState::resume() {
    logger_.Log("OverworldState resumed");

    // Only consume a result this state is still expecting. A resume() from any
    // other cause must not re-award a castle.
    if (!awaitingBattle_ || !campaign_->result.pending) {
        awaitingBattle_ = false;
        lastTicks_      = SDL_GetTicks();
        return;
    }

    const auto r = campaign_->result;
    campaign_->result.pending = false;
    awaitingBattle_           = false;

    auto &attacker = campaign_->generals[r.attacker];
    auto &defender = campaign_->generals[r.defender];

    if (r.winner == attacker.side) {
        attacker.atCastle             = r.castle;
        attacker.toCastle             = -1;
        attacker.troops               = r.winnerTroops;
        defender.alive                = false;
        campaign_->castles[r.castle].owner = attacker.side;
        RefreshCastleSprite(r.castle);
    } else {
        // The attack failed: the defender holds, and the attacker is destroyed
        // rather than retreating, so a losing general cannot immediately try
        // the same castle again.
        defender.troops = r.winnerTroops;
        attacker.alive  = false;
    }

    SyncArmyMarkers();
    registry_.Update();

    // SDL_GetTicks kept running while the battle was on top. Without this the
    // first update() back here would see the whole battle's duration as one
    // frame and advance several days at once.
    lastTicks_ = SDL_GetTicks();

    if (MaybeEndCampaign()) {
        return;
    }

    CycleSelection(0);
}

void OverworldState::processInput() {
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

        switch (event.key.keysym.sym) {
        case SDLK_ESCAPE:
            isRunning_ = false;
            return;
        case SDLK_LEFT:
            CycleSelection(-1);
            break;
        case SDLK_RIGHT:
            CycleSelection(1);
            break;
        case SDLK_UP:
            CycleDestination(-1);
            break;
        case SDLK_DOWN:
            CycleDestination(1);
            break;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
        case SDLK_SPACE:
            if (selected_ >= 0 && !destinations_.empty()) {
                StartMarch(selected_, destinations_[destSlot_]);
                CycleSelection(0);
            }
            break;
        default:
            break;
        }
    }
}

void OverworldState::update() {
    gamepad_->Update();
    if (leaving_) {
        return;
    }

    if (gamepad_->PressedBack()) {
        isRunning_ = false;
        return;
    }
    // LB/RB pick the general, the d-pad picks where it goes. Left/right on the
    // d-pad stays wired to the same thing as LB/RB so the pad matches the
    // keyboard, where the arrow keys are all there is.
    if (gamepad_->PressedLB() || gamepad_->PressedLeft()) {
        CycleSelection(-1);
    }
    if (gamepad_->PressedRB() || gamepad_->PressedRight()) {
        CycleSelection(1);
    }
    if (gamepad_->PressedUp()) {
        CycleDestination(-1);
    }
    if (gamepad_->PressedDown()) {
        CycleDestination(1);
    }
    if (gamepad_->PressedA() || gamepad_->PressedStart()) {
        if (selected_ >= 0 && !destinations_.empty()) {
            StartMarch(selected_, destinations_[destSlot_]);
            CycleSelection(0);
        }
    }

    const Uint32 now = SDL_GetTicks();
    // Clamped. An unclamped delta turns any event-loop stall -- dragging the
    // window, the machine sleeping, a slow first frame after the battle pops --
    // into a burst of days advancing in one tick, which teleports every marching
    // army to its destination at once. MAX_DELTA_MS of real time is the most the
    // campaign will absorb from a single frame; the rest is dropped.
    const Uint32 delta = std::min(now - lastTicks_, MAX_DELTA_MS);
    lastTicks_         = now;

    dayAccumMs_ += delta;
    while (dayAccumMs_ >= DAY_MS) {
        dayAccumMs_ -= DAY_MS;
        AdvanceDay();
    }

    CheckArrivals();
    if (leaving_ || awaitingBattle_) {
        return;
    }

    // Ownership can change in CheckArrivals without a battle, so the end of the
    // campaign has to be checked here as well as in resume().
    if (MaybeEndCampaign()) {
        return;
    }

    SyncArmyMarkers();
    registry_.Update();
    registry_.GetSystem<AnimationSystem>().Update();
}

void OverworldState::RenderHud() {
    SDL_Texture *digits = assetStore_->GetTexture("digits");

    // Day counter, top-left.
    ui::DrawPanel(renderer_, 12, 12, 200, 48);
    ui::DrawTexture(renderer_, assetStore_->GetTexture("day"), 24, 22, 0.8f);
    ui::DrawNumber(renderer_, digits, campaign_->day, 96, 20, 0.8f);

    // Castle tally, top-right.
    const int blue = campaign_->CountOwned(world::Owner::Blue);
    const int red  = campaign_->CountOwned(world::Owner::Red);
    ui::DrawPanel(renderer_, windowWidth_ - 172, 12, 160, 48);
    SDL_SetRenderDrawColor(renderer_, 90, 140, 230, 255);
    SDL_Rect blueSwatch{windowWidth_ - 160, 26, 20, 20};
    SDL_RenderFillRect(renderer_, &blueSwatch);
    ui::DrawNumber(renderer_, digits, blue, windowWidth_ - 132, 20, 0.8f);
    SDL_SetRenderDrawColor(renderer_, 210, 80, 80, 255);
    SDL_Rect redSwatch{windowWidth_ - 84, 26, 20, 20};
    SDL_RenderFillRect(renderer_, &redSwatch);
    ui::DrawNumber(renderer_, digits, red, windowWidth_ - 56, 20, 0.8f);

    // Selected general and its destination.
    if (selected_ >= 0) {
        const auto &g = campaign_->generals[selected_];
        const int panelY = windowHeight_ - 116;
        ui::DrawPanel(renderer_, 12, panelY, 330, 66);

        // Scaled against the strongest army any general starts with, so a full
        // bar means "as strong as anyone began". The old divisor was a bare 80,
        // which no general ever reaches -- the bar could never read more than
        // about two thirds and looked broken at full strength.
        ui::DrawNumber(renderer_, digits, g.troops, 26, panelY + 16, 0.9f);
        ui::DrawBar(renderer_, 110, panelY + 20, 210, 26,
                    g.troops / static_cast<float>(world::kMaxStartTroops),
                    ui::BlueColour());

        // Marching-order marker: a ring on the castle currently chosen.
        if (!destinations_.empty()) {
            const glm::vec2 p = CastlePixel(destinations_[destSlot_]);
            SDL_SetRenderDrawColor(renderer_, 255, 220, 90, 255);
            SDL_Rect ring{static_cast<int>(p.x) + 18,
                          static_cast<int>(p.y) + 12,
                          static_cast<int>(kCastleW * kCastleScale) - 36,
                          static_cast<int>(kCastleH * kCastleScale) - 24};
            SDL_RenderDrawRect(renderer_, &ring);
            ring.x -= 2; ring.y -= 2; ring.w += 4; ring.h += 4;
            SDL_RenderDrawRect(renderer_, &ring);
        }

        // And a marker on the selected general itself.
        const glm::vec2 gp = GeneralPixel(selected_);
        SDL_SetRenderDrawColor(renderer_, 130, 220, 255, 255);
        SDL_Rect sel{static_cast<int>(gp.x) + 10, static_cast<int>(gp.y) + 4,
                     static_cast<int>(kCastleW * kCastleScale) - 20, 6};
        SDL_RenderFillRect(renderer_, &sel);
    }

    ui::DrawTextureCentred(renderer_, assetStore_->GetTexture("hint_map"),
                           windowWidth_ / 2, windowHeight_ - 36, 0.9f);
}

void OverworldState::render() {
    SDL_SetRenderDrawColor(renderer_, 40, 90, 130, 255);
    SDL_RenderClear(renderer_);

    registry_.GetSystem<RenderSystem>().Update(renderer_, *assetStore_);
    RenderHud();

    SDL_RenderPresent(renderer_);
}
