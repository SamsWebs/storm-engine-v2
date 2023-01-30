#include <iostream>

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_mixer.h>
#include <SDL2/SDL_ttf.h>

#include "../../../vendor/glm/glm.hpp"
#include "../../../vendor/imgui/imgui.h"

int main() {
  // Tries to create a vector2 and normalize it with the GLM library
  glm::vec2 velocity = glm::vec2(5.0, -2.5);
  velocity = glm::normalize(velocity);

  // Tries to initialize SDL
  SDL_Init(SDL_INIT_EVERYTHING);

  std::cout << "Yay! Dependencies work correctly." << std::endl;
  return 0;
}
