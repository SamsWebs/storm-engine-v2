#pragma once

// ── Touch input primitives (SDL-free) ───────────────────────────────────────
// Shared building blocks for on-screen controls: a rectangular hit zone and a
// finger position, both in the game's logical (letterboxed) coordinate space.
// The caller reads raw touches from SDL, converts them into logical points, and
// feeds them here — keeping all the layout/hit-test math pure and testable.
//
// A simple three-zone scheme (◀ ▶ move, one action) lives here too; the fuller
// d-pad + action-diamond layout is in virtualGamepad.h.

struct TouchZone {
    float x = 0.f, y = 0.f, w = 0.f, h = 0.f;
    bool contains(float px, float py) const {
        return px >= x && px <= x + w && py >= y && py <= y + h;
    }
};

struct TouchPoint {
    float x = 0.f;
    float y = 0.f;
};

struct TouchZones {
    TouchZone left, right, jump;
};

struct TouchInput {
    bool left = false, right = false, jump = false;
};

// Two movement pads bottom-left, one action pad bottom-right, scaled from the
// window size with a small margin.
inline TouchZones MakeDefaultZones(float windowW, float windowH) {
    float pad    = windowH * 0.22f;
    float margin = windowH * 0.04f;
    float y      = windowH - pad - margin;

    TouchZones z;
    z.left  = {margin, y, pad, pad};
    z.right = {margin + pad + margin, y, pad, pad};
    z.jump  = {windowW - pad - margin, y, pad, pad};
    return z;
}

// Any finger inside a zone holds that control; multiple fingers are fine.
inline TouchInput EvalTouches(const TouchZones &zones,
                              const TouchPoint *points, int count) {
    TouchInput in;
    for (int i = 0; i < count; ++i) {
        if (zones.left.contains(points[i].x, points[i].y))  in.left  = true;
        if (zones.right.contains(points[i].x, points[i].y)) in.right = true;
        if (zones.jump.contains(points[i].x, points[i].y))  in.jump  = true;
    }
    return in;
}
