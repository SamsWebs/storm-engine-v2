#pragma once

// Which team/role the skater belongs to
enum class SkaterTeam { Player = 0, AISkater = 1, AIGoalie = 2 };

struct SkaterComponent {
    SkaterTeam team;
    float      speed;

    SkaterComponent() : team{SkaterTeam::Player}, speed{200.f} {}
    SkaterComponent(SkaterTeam team, float speed)
        : team{team}, speed{speed} {}
};
