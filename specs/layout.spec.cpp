#include "../common/ecs.h"
#include "../common/tilemapLoader.h"
#include <igloo/igloo_alt.h>

using namespace igloo;
using namespace storm;

// These sizes are ABI. Games allocate Registry and AssetStore themselves and
// iterate std::vector<Tile> directly, so a size change is emitted at *their*
// call site and overflows *their* allocation, with no diagnostic anywhere.
// That is how 1.3.0's AssetStore change (112 -> 208) reached users.
//
// A failure here is not a bug in this file. It means a public type changed
// size: decide whether that was intended, and if it was, update the number
// here in the same commit and say so in the commit message.
//
// Measured on x86-64, g++, -std=c++17.
Describe(LayoutSpec) {
  It(pins_the_sizes_that_are_abi) {
    Assert::That(sizeof(Registry), Equals(static_cast<std::size_t>(488)));
    Assert::That(sizeof(Entity), Equals(static_cast<std::size_t>(24)));
    Assert::That(sizeof(System), Equals(static_cast<std::size_t>(40)));
    Assert::That(sizeof(Signature), Equals(static_cast<std::size_t>(8)));

    // The size pin above cannot see MAX_COMPONENTS: sizeof(std::bitset<N>) is
    // 8 for every N from 1 to 64, so it reads the same at 32 and at 64. Pin
    // the value itself, because it is the one ABI-relevant constant no size
    // check can catch. If this ever goes past 64 the bitset becomes 16 bytes
    // and the Registry and System pins move with it.
    Assert::That(static_cast<unsigned int>(MAX_COMPONENTS), Equals(64u));
    Assert::That(sizeof(Tile), Equals(static_cast<std::size_t>(104)));
  };
};
