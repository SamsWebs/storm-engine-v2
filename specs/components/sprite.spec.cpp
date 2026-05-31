#include <SDL2/SDL.h>
#include <igloo/igloo_alt.h>
#include <string>

#include "../../common/components/sprite.h"

using namespace igloo;

Describe(SpriteComponentSpec){
    It(should_initialize_with_default_values){// Arrange & Act
                                              SpriteComponent sprite;

// Assert
Assert::That(sprite.assetId, Equals(""));
Assert::That(sprite.width, Equals(0));
Assert::That(sprite.height, Equals(0));
Assert::That(sprite.zIndex, Equals(0));
Assert::That(sprite.flip, Equals(SDL_FLIP_NONE));
// Assert::That(sprite.isFixed, Equals(false));
Assert::That(sprite.srcRect.x, Equals(0));
Assert::That(sprite.srcRect.y, Equals(0));
Assert::That(sprite.srcRect.w, Equals(0));
Assert::That(sprite.srcRect.h, Equals(0));
Assert::That(sprite.offset, Equals(glm::vec2(0, 0)));
}

It(should_initialize_with_custom_values) {
  // Arrange & Act
  std::string assetId = "player_sprite";
  int width = 100;
  int height = 100;
  int zIndex = 2;
  bool isFixed = false;
  int srcRectX = 10;
  int srcRectY = 10;
  SpriteComponent sprite(assetId, width, height, zIndex, isFixed, srcRectX,
                         srcRectY);

  // Assert
  Assert::That(sprite.assetId, Equals(assetId));
  Assert::That(sprite.width, Equals(width));
  Assert::That(sprite.height, Equals(height));
  Assert::That(sprite.zIndex, Equals(zIndex));
  Assert::That(sprite.srcRect.x, Equals(srcRectX));
  Assert::That(sprite.srcRect.y, Equals(srcRectY));
  Assert::That(sprite.srcRect.w, Equals(width));
  Assert::That(sprite.srcRect.h, Equals(height));
}
}
;
