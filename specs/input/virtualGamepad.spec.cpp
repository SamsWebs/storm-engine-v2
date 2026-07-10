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
            TouchPoint p[] = {{l.dpadCx + l.dpadRadius * 0.6f,
                               l.dpadCy - l.dpadRadius * 0.6f}};
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
            Assert::That(l.btnX.y < l.btnB.y, Equals(true)); // X above B
            Assert::That(l.btnY.x < l.btnA.x, Equals(true)); // Y left of A
        };
    };
};
