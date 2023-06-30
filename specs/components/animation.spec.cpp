#include <SDL2/SDL.h>
#include <igloo/igloo_alt.h>

#include "../../common/components/animation.h"

using namespace igloo;

Describe(AnimationComponentSpec){
    It(should_initialize_with_default_values){// Arrange & Act
                                              AnimationComponent animation;

// Assert
Assert::That(animation.numFrames, Equals(1));
Assert::That(animation.currentFrame, Equals(1));
Assert::That(animation.frameSpeedRate, Equals(1));
Assert::That(animation.isLoop, IsTrue());
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
  Assert::That(animation.isLoop, IsFalse());
}
}
;
