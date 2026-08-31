#pragma once

#include <cmath>

#include "touchControls.h" // TouchZone / TouchPoint

namespace storm {

// ── Virtual gamepad (SDL-free) ──────────────────────────────────────────────
// The standard mobile layout: a circular d-pad bottom-left (up/down/left/right
// via angle sectors, so a diagonal push sets two flags) and four action buttons
// in a diamond bottom-right, lettered Xbox-style by default (Y top, X left,
// B right, A bottom) or SNES-style on request — see VPadStyle.
//
// Pure logic: MakeVPadLayout(w, h) positions everything from the window size
// and EvalVPad(layout, fingers) maps touches onto the button/direction flags.
// The game supplies fingers in its logical coordinate space and decides what
// each button does; the render layer draws the zones from the same layout.

struct VPadState {
  bool up = false, down = false, left = false, right = false;
  bool a = false, b = false, x = false, y = false;
};

struct VPadLayout {
  float dpadCx = 0.f, dpadCy = 0.f; // d-pad centre
  float dpadRadius = 0.f;           // outer touch radius
  float dpadDead = 0.f;             // inner deadzone radius
  TouchZone btnA, btnB, btnX, btnY; // action diamond
};

// Which letter sits at which position in the action diamond. The four
// positions are identical either way — only the lettering differs, so a game
// binding to `state.a` gets the same button under a different thumb.
//
//        Xbox                    Snes
//        (Y)                     (X)
//   (X)       (B)           (Y)       (A)
//        (A)                     (B)
enum class VPadStyle { Xbox, Snes };

// D-pad evaluation: 8-way sectors around the centre. A component registers when
// it exceeds tan(22.5°) of the other, so straight pushes give one flag and
// diagonals give two.
inline void DpadFromPoint(const VPadLayout &l, float px, float py,
                          VPadState &s) {
  float dx = px - l.dpadCx, dy = py - l.dpadCy;
  float dist2 = dx * dx + dy * dy;
  if (dist2 < l.dpadDead * l.dpadDead || dist2 > l.dpadRadius * l.dpadRadius)
    return;
  constexpr float TAN_22_5 = 0.4142f;
  if (std::fabs(dx) > TAN_22_5 * std::fabs(dy)) {
    if (dx > 0.f)
      s.right = true;
    else
      s.left = true;
  }
  if (std::fabs(dy) > TAN_22_5 * std::fabs(dx)) {
    if (dy > 0.f)
      s.down = true;
    else
      s.up = true;
  }
}

// Layout scaled from the window size: d-pad bottom-left, action diamond
// mirrored bottom-right. Style picks the lettering; positions do not move.
inline VPadLayout MakeVPadLayout(float w, float h,
                                 VPadStyle style = VPadStyle::Xbox) {
  VPadLayout l;
  float margin = h * 0.04f;
  float radius = h * 0.20f;

  l.dpadCx = margin + radius;
  l.dpadCy = h - margin - radius;
  l.dpadRadius = radius;
  l.dpadDead = radius * 0.25f;

  float cx = w - margin - radius;
  float cy = h - margin - radius;
  float s = radius * 0.62f; // button side
  float o = radius * 0.68f; // offset from cluster centre

  TouchZone top = {cx - s / 2.f, cy - o - s / 2.f, s, s};
  TouchZone bottom = {cx - s / 2.f, cy + o - s / 2.f, s, s};
  TouchZone left = {cx - o - s / 2.f, cy - s / 2.f, s, s};
  TouchZone right = {cx + o - s / 2.f, cy - s / 2.f, s, s};

  if (style == VPadStyle::Snes) {
    l.btnX = top;
    l.btnY = left;
    l.btnA = right;
    l.btnB = bottom;
  } else {
    l.btnY = top;
    l.btnX = left;
    l.btnB = right;
    l.btnA = bottom;
  }
  return l;
}

inline VPadState EvalVPad(const VPadLayout &l, const TouchPoint *points,
                          int count) {
  VPadState s;
  for (int i = 0; i < count; ++i) {
    DpadFromPoint(l, points[i].x, points[i].y, s);
    if (l.btnA.contains(points[i].x, points[i].y))
      s.a = true;
    if (l.btnB.contains(points[i].x, points[i].y))
      s.b = true;
    if (l.btnX.contains(points[i].x, points[i].y))
      s.x = true;
    if (l.btnY.contains(points[i].x, points[i].y))
      s.y = true;
  }
  return s;
}

} // namespace storm
