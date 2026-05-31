#pragma once

#include <stormengine2/ecs.h>
#include <stormengine2/components/transform.h>

#include "../components/puckComponent.h"

// Rink bounds (must match PlayState constants)
constexpr float RL = 62.f;   // rink left wall  (inner)
constexpr float RT = 62.f;   // rink top wall   (inner)
constexpr float RR = 738.f;  // rink right wall (inner)
constexpr float RB = 538.f;  // rink bottom wall(inner)

constexpr float PUCK_HALF = 8.f; // half of PUCK_SIZE (16px)

// HockeyPhysicsSystem: moves the free puck and bounces it off rink walls.
// Goal detection is NOT done here — PlayState reads puck position directly.
class HockeyPhysicsSystem : public System {
public:
    HockeyPhysicsSystem() {
        RequireComponent<TransformComponent>();
        RequireComponent<PuckComponent>();
    }

    void Update(double dt) {
        for (auto &entity : GetSystemEntities()) {
            auto &transform = entity.GetComponent<TransformComponent>();
            auto &puck      = entity.GetComponent<PuckComponent>();

            // If held by someone, skip physics
            if (puck.ownerTag != -1) continue;

            // Move puck
            transform.position.x += puck.velocity.x * static_cast<float>(dt);
            transform.position.y += puck.velocity.y * static_cast<float>(dt);

            // Apply friction (exponential decay each frame)
            float drag = 1.f - puck.friction * static_cast<float>(dt);
            if (drag < 0.f) drag = 0.f;
            puck.velocity *= drag;

            // Stop very slow puck to avoid endless micro-movement
            if (glm::length(puck.velocity) < 5.f)
                puck.velocity = {0.f, 0.f};

            // Bounce off top / bottom walls
            if (transform.position.y - PUCK_HALF < RT) {
                transform.position.y = RT + PUCK_HALF;
                puck.velocity.y      = std::abs(puck.velocity.y);
            }
            if (transform.position.y + PUCK_HALF > RB) {
                transform.position.y = RB - PUCK_HALF;
                puck.velocity.y      = -std::abs(puck.velocity.y);
            }

            // Left / right walls bounce — goal zone handled in PlayState
            if (transform.position.x - PUCK_HALF < RL) {
                transform.position.x = RL + PUCK_HALF;
                puck.velocity.x      = std::abs(puck.velocity.x);
            }
            if (transform.position.x + PUCK_HALF > RR) {
                transform.position.x = RR - PUCK_HALF;
                puck.velocity.x      = -std::abs(puck.velocity.x);
            }
        }
    }
};
