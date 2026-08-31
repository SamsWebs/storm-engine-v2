#include "../common/gameStateMachine.h"
#include <igloo/igloo_alt.h>

#include <type_traits>

using namespace igloo;
using namespace storm;

// GameStateMachine holds raw GameState pointers in two vectors and deletes them
// in clean(). Copying one gave two machines owning the same pointers, and the
// second clean() freed what the first already had. The destructor is empty, so
// it never showed up at scope exit -- it needed both machines to tick, which is
// why it survived every example.
//
// A compile-time property needs a compile-time assertion; these run at build
// time and the It() block only reports that they held.
static_assert(!std::is_copy_constructible<GameStateMachine>::value,
              "GameStateMachine must not be copy constructible");
static_assert(!std::is_copy_assignable<GameStateMachine>::value,
              "GameStateMachine must not be copy assignable");

// Deleting the copy operations suppresses the implicit move operations too.
// That is intended: every game holds one by value as a member and none moves
// it, so there is no reason to hand out a second way to duplicate the
// pointers.
static_assert(!std::is_move_constructible<GameStateMachine>::value,
              "GameStateMachine must not be move constructible");
static_assert(!std::is_move_assignable<GameStateMachine>::value,
              "GameStateMachine must not be move assignable");

// Default construction is how every game creates one, and must still work.
static_assert(std::is_default_constructible<GameStateMachine>::value,
              "GameStateMachine must stay default constructible");

Describe(GameStateMachineNonCopyableSpec) {
  It(should_not_be_copyable_or_movable) {
    Assert::That(std::is_copy_constructible<GameStateMachine>::value,
                 Equals(false));
    Assert::That(std::is_copy_assignable<GameStateMachine>::value,
                 Equals(false));
    Assert::That(std::is_move_constructible<GameStateMachine>::value,
                 Equals(false));
    Assert::That(std::is_default_constructible<GameStateMachine>::value,
                 Equals(true));
  };
};
