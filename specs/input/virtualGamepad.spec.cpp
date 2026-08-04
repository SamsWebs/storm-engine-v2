#include <igloo/igloo_alt.h>

#include "../../common/input/virtualGamepad.h"

using namespace igloo;

// 640x480 logical window: d-pad centre ~(115, 365) radius 96, diamond right.
Describe(VirtualGamepadSpec) {

  Describe(Dpad) {
    It(should_report_a_straight_push_as_one_direction) {
      VPadLayout l = MakeVPadLayout(640.f, 480.f);
      TouchPoint p[] = {{l.dpadCx + l.dpadRadius * 0.7f, l.dpadCy}};
      VPadState s = EvalVPad(l, p, 1);
      Assert::That(s.right, Equals(true));
      Assert::That(s.up || s.down || s.left, Equals(false));
    };
    It(should_report_a_diagonal_as_two_directions) {
      VPadLayout l = MakeVPadLayout(640.f, 480.f);
      TouchPoint p[] = {
          {l.dpadCx + l.dpadRadius * 0.6f, l.dpadCy - l.dpadRadius * 0.6f}};
      VPadState s = EvalVPad(l, p, 1);
      Assert::That(s.right && s.up, Equals(true));
    };
    It(should_ignore_the_deadzone_centre) {
      VPadLayout l = MakeVPadLayout(640.f, 480.f);
      TouchPoint p[] = {{l.dpadCx + 2.f, l.dpadCy}};
      VPadState s = EvalVPad(l, p, 1);
      Assert::That(s.left || s.right || s.up || s.down, Equals(false));
    };
    It(should_ignore_touches_outside_the_pad_radius) {
      VPadLayout l = MakeVPadLayout(640.f, 480.f);
      TouchPoint p[] = {{l.dpadCx + l.dpadRadius * 2.f, l.dpadCy}};
      VPadState s = EvalVPad(l, p, 1);
      Assert::That(s.right, Equals(false));
    };
  };

  Describe(ActionButtons) {
    It(should_press_a_single_button) {
      VPadLayout l = MakeVPadLayout(640.f, 480.f);
      TouchPoint p[] = {{l.btnA.x + 5.f, l.btnA.y + 5.f}};
      VPadState s = EvalVPad(l, p, 1);
      Assert::That(s.a, Equals(true));
      Assert::That(s.b || s.x || s.y, Equals(false));
    };
    It(should_hold_a_direction_and_a_button_with_two_fingers) {
      VPadLayout l = MakeVPadLayout(640.f, 480.f);
      TouchPoint p[] = {{l.dpadCx - l.dpadRadius * 0.7f, l.dpadCy},
                        {l.btnB.x + 5.f, l.btnB.y + 5.f}};
      VPadState s = EvalVPad(l, p, 2);
      Assert::That(s.left && s.b, Equals(true));
    };
    It(should_keep_the_diamond_positions_distinct) {
      VPadLayout l = MakeVPadLayout(640.f, 480.f);
      Assert::That(l.btnY.y < l.btnA.y, Equals(true)); // Y above A
      Assert::That(l.btnX.x < l.btnB.x, Equals(true)); // X left of B
    };
    It(should_letter_the_diamond_xbox_style_by_default) {
      // Y top, X left, B right, A bottom.
      VPadLayout l = MakeVPadLayout(640.f, 480.f);
      Assert::That(l.btnY.y < l.btnX.y, Equals(true)); // Y above the sides
      Assert::That(l.btnY.y < l.btnB.y, Equals(true));
      Assert::That(l.btnA.y > l.btnX.y, Equals(true)); // A below the sides
      Assert::That(l.btnA.y > l.btnB.y, Equals(true));
      Assert::That(l.btnX.x < l.btnY.x, Equals(true)); // X leftmost
      Assert::That(l.btnB.x > l.btnY.x, Equals(true)); // B rightmost
    };
    It(should_letter_the_diamond_snes_style_when_asked) {
      // X top, Y left, A right, B bottom — the pre-1.2.2 arrangement.
      VPadLayout l = MakeVPadLayout(640.f, 480.f, VPadStyle::Snes);
      Assert::That(l.btnX.y < l.btnY.y, Equals(true)); // X above the sides
      Assert::That(l.btnX.y < l.btnA.y, Equals(true));
      Assert::That(l.btnB.y > l.btnY.y, Equals(true)); // B below the sides
      Assert::That(l.btnB.y > l.btnA.y, Equals(true));
      Assert::That(l.btnY.x < l.btnX.x, Equals(true)); // Y leftmost
      Assert::That(l.btnA.x > l.btnX.x, Equals(true)); // A rightmost
    };
    It(should_put_the_same_four_positions_under_both_styles) {
      // Only the lettering differs; the touch targets are identical.
      VPadLayout x = MakeVPadLayout(640.f, 480.f, VPadStyle::Xbox);
      VPadLayout s = MakeVPadLayout(640.f, 480.f, VPadStyle::Snes);
      Assert::That(x.btnY.x, Equals(s.btnX.x)); // top
      Assert::That(x.btnY.y, Equals(s.btnX.y));
      Assert::That(x.btnA.x, Equals(s.btnB.x)); // bottom
      Assert::That(x.btnA.y, Equals(s.btnB.y));
      Assert::That(x.btnX.x, Equals(s.btnY.x)); // left
      Assert::That(x.btnB.x, Equals(s.btnA.x)); // right
    };
  };
};
