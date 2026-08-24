#pragma once

enum class Direction { Up, Down, Left, Right };

struct PlayerComponent {
  float moveSpeed = 120.0f;
  Direction facing = Direction::Down;
  bool isMoving = false;

  // Walk animation (0–3 cycling through the 4 walk frames for current
  // direction)
  int walkFrame = 0;
  float animTimer = 0.0f;
  float animInterval = 0.15f; // seconds per walk frame
};
