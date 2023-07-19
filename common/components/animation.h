#pragma once

#include <SDL2/SDL.h>

struct AnimationComponent {
  int numFrames;
  int currentFrame;
  int frameSpeedRate;
  bool isLoop;
  int startTime;
  bool isVertical;
  int frameOffset;
  int lastFrame;

  AnimationComponent(int numFrames = 1, int frameSpeedRate = 1,
                     bool isLoop = true, bool isVertical = true,
                     int frameOffset = 0)
      : numFrames{numFrames}, currentFrame{1}, frameSpeedRate{frameSpeedRate},
        isLoop{isLoop}, isVertical{isVertical}, frameOffset{frameOffset} {
    startTime = SDL_GetTicks();
    lastFrame = 0;
  }
};