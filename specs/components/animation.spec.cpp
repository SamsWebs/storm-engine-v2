#include <SDL2/SDL.h>
#include <igloo/igloo_alt.h>

#include "../../common/components/animation.h"

using namespace igloo;
using namespace storm;

Describe(AnimationComponentSpec){It(should_initialize_with_default_values){

    // Arrange & Act
    AnimationComponent animation;

// Assert
Assert::That(animation.numFrames, Equals(1));
Assert::That(animation.currentFrame, Equals(1));
Assert::That(animation.frameSpeedRate, Equals(1));
Assert::That(animation.isLooped, IsTrue());
}

It(should_initialize_with_custom_values) {
  // Arrange & Act
  int numFrames = 10;
  int frameSpeedRate = 2;
  bool isLoop = false;
  AnimationComponent animation(numFrames, frameSpeedRate, isLoop);

  // Assert
  Assert::That(animation.numFrames, Equals(numFrames));
  Assert::That(animation.currentFrame, Equals(1));
  Assert::That(animation.frameSpeedRate, Equals(frameSpeedRate));
  Assert::That(animation.isLooped, IsTrue());
}
}
;

// Deeper coverage: the constructor signature is
//   AnimationComponent(numFrames, frameSpeedRate, vertical, isLooped, frameOffset)
// so vertical and isLooped are distinct positional args. These specs pin down
// each field explicitly to guard against arg-order mistakes.
Describe(AnimationComponentFieldsSpec) {
  It(should_default_vertical_to_true) {
    AnimationComponent animation;
    Assert::That(animation.vertical, IsTrue());
  };

  It(should_default_frame_offset_to_zero) {
    AnimationComponent animation;
    Assert::That(animation.frameOffset, Equals(0));
  };

  It(should_default_last_frame_to_zero) {
    AnimationComponent animation;
    Assert::That(animation.lastFrame, Equals(0));
  };

  It(should_set_vertical_from_the_third_argument) {
    AnimationComponent animation(4, 2, false);
    Assert::That(animation.vertical, IsFalse());
  };

  It(should_set_is_looped_from_the_fourth_argument) {
    AnimationComponent animation(4, 2, true, false);
    Assert::That(animation.isLooped, IsFalse());
  };

  It(should_set_frame_offset_from_the_fifth_argument) {
    AnimationComponent animation(4, 2, true, true, 7);
    Assert::That(animation.frameOffset, Equals(7));
  };

  It(should_always_start_on_frame_one) {
    AnimationComponent animation(10, 5, false, false, 3);
    Assert::That(animation.currentFrame, Equals(1));
  };
};
