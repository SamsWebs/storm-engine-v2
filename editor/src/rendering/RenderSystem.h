#pragma once

#include <vector>

#include "../AssetManager.h"

#include <stormengine2/components/sprite.h>
#include <stormengine2/components/transform.h>
#include <stormengine2/ecs.h>

using namespace storm;

class RenderSystem : public System {
public:
  RenderSystem();

  void Update(struct SDL_Renderer *renderer,
              std::unique_ptr<class AssetManager> &assetManager,
              struct SDL_Rect &camera, const float &zoom);
};