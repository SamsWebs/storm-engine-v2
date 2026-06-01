#pragma once
#include <string>

#include <SDL2/SDL.h>
#include <glm/glm.hpp>

struct SpriteComponent {
  std::string assetId;
  int width;
  int height;
  int zIndex;
  bool isFixed;
  SDL_RendererFlip flip;
  SDL_Rect srcRect;
  glm::vec2 offset;

  SpriteComponent(const std::string &assetId = "", int width = 0,
                  int height = 0, int zIndex = 0, bool isfixed = false,
                  int srcRectX = 0, int srcRectY = 0,
                  glm::vec2 offset = glm::vec2(0))
      : assetId{assetId}, width{width}, height{height}, zIndex{zIndex},
        isFixed{isfixed}, srcRect{srcRectX, srcRectY, width, height},
        offset{offset} {
    flip = SDL_FLIP_NONE;
  }
};