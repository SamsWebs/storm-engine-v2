#pragma once

#include "playerComponent.h"
#include <string>

struct NpcComponent {
  std::string name;
  std::string dialogue;
  Direction facing = Direction::Down;
  float interactDist = 48.0f; // pixels — how close the player must be
};
