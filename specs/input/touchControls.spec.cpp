#include <igloo/igloo_alt.h>

#include "../../common/input/touchControls.h"

using namespace igloo;
using namespace storm;

// 800x480 window: pads are 105.6 square; left at x=19.2, right at x=144,
// jump at x=675.2, all at y=355.2.
Describe(TouchControlsSpec) {

    It(should_lay_the_zones_out_along_the_bottom) {
        TouchZones z = MakeDefaultZones(800.f, 480.f);
        Assert::That(z.left.y > 300.f, Equals(true));
        Assert::That(z.jump.x > 600.f, Equals(true));                // bottom-right
        Assert::That(z.right.x > z.left.x + z.left.w, Equals(true)); // no overlap
    };

    It(should_hold_left_when_a_finger_is_on_the_left_pad) {
        TouchZones z = MakeDefaultZones(800.f, 480.f);
        TouchPoint p[] = {{z.left.x + 10.f, z.left.y + 10.f}};
        TouchInput in = EvalTouches(z, p, 1);
        Assert::That(in.left, Equals(true));
        Assert::That(in.right, Equals(false));
        Assert::That(in.jump, Equals(false));
    };

    It(should_hold_move_and_jump_with_two_fingers) {
        TouchZones z = MakeDefaultZones(800.f, 480.f);
        TouchPoint p[] = {{z.right.x + 10.f, z.right.y + 10.f},
                          {z.jump.x + 10.f,  z.jump.y + 10.f}};
        TouchInput in = EvalTouches(z, p, 2);
        Assert::That(in.right, Equals(true));
        Assert::That(in.jump, Equals(true));
    };

    It(should_ignore_touches_outside_every_zone) {
        TouchZones z = MakeDefaultZones(800.f, 480.f);
        TouchPoint p[] = {{400.f, 100.f}}; // mid-screen
        TouchInput in = EvalTouches(z, p, 1);
        Assert::That(in.left || in.right || in.jump, Equals(false));
    };

    It(should_report_nothing_with_no_fingers) {
        TouchZones z = MakeDefaultZones(800.f, 480.f);
        TouchInput in = EvalTouches(z, nullptr, 0);
        Assert::That(in.left || in.right || in.jump, Equals(false));
    };
};
