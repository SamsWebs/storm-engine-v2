#pragma once

// ── Pure virtual-gamepad logic (SDL-free) ───────────────────────────────────
// Three on-screen zones: ◀ and ▶ in the bottom-left corner, A (jump) in the
// bottom-right. EvalTouches maps the current finger positions (in window
// pixels) onto held controls; the render layer draws the zones and PlayState
// edge-detects jump itself.

struct TouchZone {
    float x = 0.f, y = 0.f, w = 0.f, h = 0.f;
    bool contains(float px, float py) const {
        return px >= x && px <= x + w && py >= y && py <= y + h;
    }
};

struct TouchZones {
    TouchZone left, right, jump;
};

struct TouchInput {
    bool left = false, right = false, jump = false;
};

// Zone layout scaled from the window size: two movement pads bottom-left, one
// jump pad bottom-right, with a small margin.
inline TouchZones MakeDefaultZones(float windowW, float windowH) {
    float pad    = windowH * 0.22f; // pad side length
    float margin = windowH * 0.04f;
    float y      = windowH - pad - margin;

    TouchZones z;
    z.left  = {margin, y, pad, pad};
    z.right = {margin + pad + margin, y, pad, pad};
    z.jump  = {windowW - pad - margin, y, pad, pad};
    return z;
}

struct TouchPoint {
    float x = 0.f, y = 0.f;
};

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
