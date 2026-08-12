#include "world.h"

namespace world {

void Campaign::Reset() {
    day    = 1;
    result = BattleResult{};

    // Castle tiles are hand-placed to match the road graph in world.h: two rows
    // of three, wide enough apart that the 320x256 castle sprites do not touch.
    // The map is 16x12 tiles of 64px.
    castles[0] = {{2, 2},  Owner::Blue};
    castles[1] = {{7, 2},  Owner::Neutral};
    castles[2] = {{12, 2}, Owner::Red};
    castles[3] = {{2, 8},  Owner::Blue};
    castles[4] = {{7, 8},  Owner::Neutral};
    castles[5] = {{12, 8}, Owner::Red};

    // Three generals a side, one of each troop type, so the counter triangle is
    // reachable from the opening position rather than needing a lucky matchup.
    generals[0] = {Owner::Blue, Troop::Warrior,  50, 0, -1, 0.0f, true};
    generals[1] = {Owner::Blue, Troop::Archer,   45, 0, -1, 0.0f, true};
    generals[2] = {Owner::Blue, Troop::Spearman, 40, 3, -1, 0.0f, true};

    generals[3] = {Owner::Red,  Troop::Warrior,  50, 2, -1, 0.0f, true};
    generals[4] = {Owner::Red,  Troop::Archer,   45, 5, -1, 0.0f, true};
    generals[5] = {Owner::Red,  Troop::Spearman, 40, 5, -1, 0.0f, true};
}

} // namespace world
