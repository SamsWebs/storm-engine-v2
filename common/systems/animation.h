#pragma once

#include "../components/animation.h"
#include "../components/sprite.h"
#include "../ecs.h"
#include <SDL2/SDL.h>
#include <algorithm>

namespace storm {

class AnimationSystem : public System {
public:
  AnimationSystem() {
    RequireComponent<SpriteComponent>();
    RequireComponent<AnimationComponent>();
  }

  void Update() {
    for (auto &entity : GetSystemEntities()) {
      auto &animation = entity.GetComponent<AnimationComponent>();
      auto &sprite = entity.GetComponent<SpriteComponent>();

      if (animation.numFrames <= 0)
        continue; // no frames: nothing to animate

      int frame = (SDL_GetTicks() - animation.startTime) *
                  animation.frameSpeedRate / 1000;
      if (animation.isLooped) {
        animation.currentFrame = frame % animation.numFrames;
      } else {
        // Non-looped: advance to the end and stop on the last frame.
        int last = animation.lastFrame > 0 ? animation.lastFrame
                                           : animation.numFrames - 1;
        animation.currentFrame = std::min(
            std::max(frame, 0), std::min(last, animation.numFrames - 1));
      }

      int sheetFrame = animation.frameOffset + animation.currentFrame;
      if (animation.vertical)
        sprite.srcRect.y = sheetFrame * sprite.height;
      else
        sprite.srcRect.x = sheetFrame * sprite.width;
    }
  }
};
} // namespace storm
