#pragma once

#include <glm/glm.hpp>

#include <array>
#include <string>

// The campaign state, shared between the overworld and the battle.
//
// This lives outside both states on purpose. OverworldState pushes BattleState
// on top of itself rather than replacing it, so the overworld is still alive --
// with its Registry, its entities and its day counter -- while the battle runs.
// The battle needs to read which armies met and write back who won, and a plain
// struct owned by Game is the honest way to share that: no back-pointer into a
// state that may be mid-teardown, and no copy to keep in sync.
namespace world {

enum class Owner { Neutral, Blue, Red };

// The rock-paper-scissors triangle is the whole tactical layer, so it needs at
// least three types to close. Tiny Swords' free pack ships combat animations
// for Warrior and Archer only -- the Lancer is on a 320px frame instead of
// 192px and the Monk has no attack -- so Spearman reuses the Warrior art with a
// different multiplier. That is honest for a demo: the counter table is the
// mechanic, and the art is the same soldier holding the line differently.
enum class Troop { Warrior, Archer, Spearman };

constexpr int kCastleCount = 6;
constexpr int kGeneralCount = 6;

// Multiplier applied to `attacker`'s damage when fighting `defender`.
// Warrior > Archer > Spearman > Warrior.
inline float Counter(Troop attacker, Troop defender) {
    const bool strong = (attacker == Troop::Warrior  && defender == Troop::Archer)   ||
                        (attacker == Troop::Archer   && defender == Troop::Spearman) ||
                        (attacker == Troop::Spearman && defender == Troop::Warrior);
    const bool weak   = (defender == Troop::Warrior  && attacker == Troop::Archer)   ||
                        (defender == Troop::Archer   && attacker == Troop::Spearman) ||
                        (defender == Troop::Spearman && attacker == Troop::Warrior);
    // 1.5x, not 1.1x: a ten-percent edge is invisible in a battle that lasts
    // twenty seconds, and an invisible mechanic may as well not be there.
    if (strong) return 1.5f;
    if (weak)   return 1.0f / 1.5f;
    return 1.0f;
}

inline const char *TroopName(Troop t) {
    switch (t) {
    case Troop::Warrior:  return "Warrior";
    case Troop::Archer:   return "Archer";
    case Troop::Spearman: return "Spearman";
    }
    return "?";
}

// Which sprite folder a side draws from. Neutral castles borrow the third set.
inline const char *OwnerDir(Owner o) {
    switch (o) {
    case Owner::Blue: return "blue";
    case Owner::Red:  return "red";
    default:          return "neutral";
    }
}

struct Castle {
    glm::ivec2 tile;      // position on the overworld grid
    Owner      owner = Owner::Neutral;
};

struct General {
    Owner  side       = Owner::Blue;
    Troop  troop      = Troop::Warrior;
    int    troops     = 0;
    int    atCastle   = 0;    // index into castles; where it sits or came from
    int    toCastle   = -1;   // -1 when idle, else the march destination
    float  marchDays  = 0.0f; // days remaining on the current march
    bool   alive      = true;
};

// Set by BattleState immediately before it pops, read by
// OverworldState::resume(). `pending` is the handshake: the overworld only
// applies a result it is still expecting, so a spurious resume() (from any
// other state being popped later) cannot re-award a castle.
struct BattleResult {
    bool  pending      = false;
    int   attacker     = -1;   // general index
    int   defender     = -1;   // general index, or -1 for an empty castle
    int   castle       = -1;
    Owner winner       = Owner::Neutral;
    int   winnerTroops = 0;
};

struct Campaign {
    std::array<Castle, kCastleCount>   castles;
    std::array<General, kGeneralCount> generals;
    BattleResult                       result;
    int                                day = 1;

    // Road graph. Authored here rather than derived from the tilemap: the map
    // is decoration, the graph is the rules, and deriving one from the other
    // would make a cosmetic tile edit silently change the campaign.
    //
    //      0 --- 1 --- 2
    //      |     |     |
    //      3 --- 4 --- 5
    bool Adjacent(int a, int b) const {
        static const int kEdges[][2] = {{0, 1}, {1, 2}, {0, 3}, {1, 4},
                                        {2, 5}, {3, 4}, {4, 5}};
        for (const auto &e : kEdges) {
            if ((e[0] == a && e[1] == b) || (e[0] == b && e[1] == a)) {
                return true;
            }
        }
        return false;
    }

    int CountOwned(Owner o) const {
        int n = 0;
        for (const auto &c : castles) {
            if (c.owner == o) ++n;
        }
        return n;
    }

    // The general sitting at a castle, or -1. Marching generals are in transit
    // and do not defend the castle they left.
    int GarrisonAt(int castle) const {
        for (int i = 0; i < kGeneralCount; ++i) {
            const auto &g = generals[i];
            if (g.alive && g.toCastle < 0 && g.atCastle == castle) {
                return i;
            }
        }
        return -1;
    }

    void Reset();
};

} // namespace world
