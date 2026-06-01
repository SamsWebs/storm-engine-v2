#pragma once

#include <glm/glm.hpp>

struct PlayerComponent {
    glm::vec2 velocity    = {0.0f, 0.0f};
    bool      isOnGround  = false;
    bool      facingRight = true;

    float moveSpeed = 180.0f;
    float jumpSpeed = -480.0f;
    float gravity   = 900.0f;
};
