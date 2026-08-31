#pragma once

#include <SDL2/SDL.h>

namespace storm {

struct AnimationComponent {
  int numFrames;
  int currentFrame;
  int frameSpeedRate;
  bool vertical;
  bool isLooped;
  int startTime;
  int frameOffset;
  int lastFrame;

  AnimationComponent(int numFrames = 1, int frameSpeedRate = 1,
                     bool vertical = true, bool isLooped = true,
                     int frameOffset = 0)
      : numFrames{numFrames}, currentFrame{1}, frameSpeedRate{frameSpeedRate},
        vertical{vertical}, isLooped{isLooped}, frameOffset{frameOffset} {
    currentFrame = 1;
    startTime = SDL_GetTicks();
    lastFrame = 0;
  }
};
} // namespace storm
