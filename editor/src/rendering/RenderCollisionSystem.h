#pragma once

#include <SDL2/SDL.h>

#include <stormengine2/components/boxCollider.h>
#include <stormengine2/components/transform.h>
#include <stormengine2/ecs.h>

#include "../utilities/Utilities.h"

using namespace storm;

class RenderCollisionSystem : public System {
public:
  RenderCollisionSystem();

  /*
   *  Update() - Renders all the colliders that are currently in the project
   */
  void Update(std::unique_ptr<SDL_Renderer, Util::SDLDestroyer> &renderer,
              SDL_Rect &camera, const float &zoom);
};