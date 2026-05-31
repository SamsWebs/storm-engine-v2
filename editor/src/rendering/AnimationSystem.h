#pragma once

#include <SDL2/SDL.h>

#include <stormengine2/components/animation.h>
#include <stormengine2/components/sprite.h>
#include <stormengine2/components/transform.h>
#include <stormengine2/ecs.h>

class AnimationSystem : public System {
public:
  AnimationSystem();

  void Update();
};