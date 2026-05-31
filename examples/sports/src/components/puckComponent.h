#pragma once

#include <glm/glm.hpp>

struct PuckComponent {
    glm::vec2 velocity   = {0.f, 0.f};
    int       ownerTag   = -1;  // -1 = free; 0 = player; 1 = AI skater; 2 = AI goalie
    float     friction   = 1.8f; // speed lost per second (units/s^2 style drag)

    PuckComponent() = default;
};
