#pragma once

#include <SDL2/SDL.h>

#include <stormengine2/components/animation.h>
#include <stormengine2/components/sprite.h>
#include <stormengine2/components/transform.h>
#include <stormengine2/ecs.h>

// Named EditorX, not X. The editor has always had its own AnimationSystem that
// does a different job from the engine's (common/systems/animation.h), and before 2.0.0
// both were `::AnimationSystem` in one linked program -- an ODR violation nothing
// diagnosed. The namespace made them distinct types, but left the editor one
// unqualified name away from ambiguity: a using-directive plus a global
// `AnimationSystem` means any translation unit that includes both headers cannot
// resolve the name. Renaming settles it rather than relying on the editor
// never including <stormengine2/systems/*.h>.
//
// No `using namespace storm;` in this header either. A using-directive in a
// header is TU-wide and transitive, so it belongs in a .cpp.
class EditorAnimationSystem : public storm::System {
public:
  EditorAnimationSystem();

  void Update();
};