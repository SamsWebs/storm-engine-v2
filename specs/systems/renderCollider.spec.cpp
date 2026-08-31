#include "../../common/systems/renderCollider.h"
#include "../support/softwareRenderer.h"
#include <igloo/igloo_alt.h>

using namespace igloo;
using namespace storm;

namespace {

// common/systems/renderCollider.h:78 — the debug outline colour.
constexpr Uint32 kGreen = 0x00FF00;

} // namespace

Describe(RenderColliderSystemSpec) {
  It(should_select_only_entities_that_have_both_a_transform_and_a_collider) {
    Registry registry;
    registry.AddSystem<RenderColliderSystem>();

    Entity collidable = registry.CreateEntity();
    collidable.AddComponent<TransformComponent>();
    collidable.AddComponent<BoxColliderComponent>();

    Entity transformOnly = registry.CreateEntity();
    transformOnly.AddComponent<TransformComponent>();

    Entity colliderOnly = registry.CreateEntity();
    colliderOnly.AddComponent<BoxColliderComponent>();

    registry.Update();

    auto &entities =
        registry.GetSystem<RenderColliderSystem>().GetSystemEntities();
    Assert::That(entities.size(), Equals(1u));
    Assert::That(entities[0].GetId(), Equals(collidable.GetId()));
  };

  It(should_outline_the_collider_at_the_transform_position) {
    SpecSurfaceTarget target(48, 48);
    Assert::That(target.IsUsable(), Equals(true));

    Registry registry;
    registry.AddSystem<RenderColliderSystem>();

    Entity entity = registry.CreateEntity();
    entity.AddComponent<TransformComponent>(glm::vec2(4, 6));
    entity.AddComponent<BoxColliderComponent>(10, 8);

    registry.Update();
    registry.GetSystem<RenderColliderSystem>().Update(target.renderer);

    Assert::That(target.RgbAt(4, 6), Equals(kGreen));   // top-left
    Assert::That(target.RgbAt(13, 6), Equals(kGreen));  // top-right
    Assert::That(target.RgbAt(4, 13), Equals(kGreen));  // bottom-left
    Assert::That(target.RgbAt(13, 13), Equals(kGreen)); // bottom-right

    // An outline, not a fill.
    Assert::That(target.RgbAt(9, 10), Equals(SpecSurfaceTarget::kNothing));
    // One column past the right edge.
    Assert::That(target.RgbAt(14, 6), Equals(SpecSurfaceTarget::kNothing));

    // Perimeter of a 10x8 rectangle: 2 * (10 + 8) - 4 shared corners.
    Assert::That(target.DrawnPixelCount(), Equals(32));
  };

  It(should_shift_the_outline_by_the_collider_offset) {
    SpecSurfaceTarget target(48, 48);
    Assert::That(target.IsUsable(), Equals(true));

    Registry registry;
    registry.AddSystem<RenderColliderSystem>();

    Entity entity = registry.CreateEntity();
    entity.AddComponent<TransformComponent>(glm::vec2(4, 6));
    entity.AddComponent<BoxColliderComponent>(10, 8, glm::vec2(3, 2));

    registry.Update();
    registry.GetSystem<RenderColliderSystem>().Update(target.renderer);

    Assert::That(target.RgbAt(7, 8), Equals(kGreen));   // (4 + 3, 6 + 2)
    Assert::That(target.RgbAt(16, 15), Equals(kGreen)); // opposite corner
    Assert::That(target.RgbAt(4, 6), Equals(SpecSurfaceTarget::kNothing));
  };

  It(should_scale_the_outline_by_the_transform_scale) {
    SpecSurfaceTarget target(48, 48);
    Assert::That(target.IsUsable(), Equals(true));

    Registry registry;
    registry.AddSystem<RenderColliderSystem>();

    Entity entity = registry.CreateEntity();
    entity.AddComponent<TransformComponent>(glm::vec2(4, 6), glm::vec2(2, 2));
    entity.AddComponent<BoxColliderComponent>(10, 8);

    registry.Update();
    registry.GetSystem<RenderColliderSystem>().Update(target.renderer);

    // 10 * 2 wide and 8 * 2 tall, still anchored at the unscaled position.
    Assert::That(target.RgbAt(4, 6), Equals(kGreen));
    Assert::That(target.RgbAt(23, 21), Equals(kGreen));
    Assert::That(target.RgbAt(24, 21), Equals(SpecSurfaceTarget::kNothing));
    Assert::That(target.DrawnPixelCount(), Equals(2 * (20 + 16) - 4));
  };

  // Unlike RenderSystem, RenderColliderSystem takes no camera and has no
  // isFixed equivalent: the debug outline is drawn in world coordinates, so a
  // panned game draws it in the wrong place. That is the current contract, not
  // an accident of this spec.
  It(should_draw_the_outline_in_world_space_with_no_camera_parameter) {
    SpecSurfaceTarget target(48, 48);
    Assert::That(target.IsUsable(), Equals(true));

    Registry registry;
    registry.AddSystem<RenderColliderSystem>();

    Entity entity = registry.CreateEntity();
    entity.AddComponent<TransformComponent>(glm::vec2(20, 20));
    entity.AddComponent<BoxColliderComponent>(4, 4);

    registry.Update();
    registry.GetSystem<RenderColliderSystem>().Update(target.renderer);

    Assert::That(target.RgbAt(20, 20), Equals(kGreen));
    Assert::That(target.DrawnPixelCount(), Equals(2 * (4 + 4) - 4));
  };

  It(should_draw_nothing_when_no_entity_matches) {
    SpecSurfaceTarget target(48, 48);
    Assert::That(target.IsUsable(), Equals(true));

    Registry registry;
    registry.AddSystem<RenderColliderSystem>();
    registry.Update();

    registry.GetSystem<RenderColliderSystem>().Update(target.renderer);

    Assert::That(
        registry.GetSystem<RenderColliderSystem>().GetSystemEntities().size(),
        Equals(0u));
    Assert::That(target.DrawnPixelCount(), Equals(0));
  };
};
