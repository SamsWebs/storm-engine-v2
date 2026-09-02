#include "../../common/systems/renderCollider.h"
#include "../support/softwareRenderer.h"
#include <cmath>
#include <limits>
#include <igloo/igloo_alt.h>

using namespace igloo;
using namespace storm;

namespace {

// common/systems/renderCollider.h:49 — the debug outline colour.
constexpr Uint32 kGreen = 0x00FF00;

// The furthest any drawn pixel sits from the circle it is meant to trace. A
// midpoint rasteriser puts every pixel within half a pixel of the ideal circle
// either side; one whole pixel of slack keeps the assertion about the SHAPE
// rather than about the algorithm, so it still fails loudly on a filled disc, a
// rectangle, or a radius resolved from the wrong axis.
constexpr float kOutlineSlack = 1.0f;

// Every drawn pixel lies on the outline of the given circle. Checking what was
// NOT drawn is the half that matters here: a fill, a box, or an outline centred
// on the transform instead of the collider's centre all pass a handful of
// spot-checked points and fail this.
bool EveryDrawnPixelIsOnTheCircle(const SpecSurfaceTarget &target, float centreX,
                                  float centreY, float radius) {
  for (int y = 0; y < target.surface->h; ++y) {
    for (int x = 0; x < target.surface->w; ++x) {
      if (target.RgbAt(x, y) == SpecSurfaceTarget::kNothing)
        continue;
      const float dx = static_cast<float>(x) - centreX;
      const float dy = static_cast<float>(y) - centreY;
      if (std::abs(std::sqrt(dx * dx + dy * dy) - radius) > kOutlineSlack)
        return false;
    }
  }
  return true;
}

} // namespace

Describe(RenderColliderSystemSpec) {
  // Membership is TransformComponent alone, matching ContactSystem, because a
  // signature is an AND and cannot say "a box collider OR a circle collider".
  // The narrowing to actual colliders happens inside Update(), so an entity
  // with a transform and no collider is a member that draws nothing.
  It(should_select_every_entity_with_a_transform_and_draw_only_the_colliders) {
    SpecSurfaceTarget target(48, 48);
    Assert::That(target.IsUsable(), Equals(true));

    Registry registry;
    registry.AddSystem<RenderColliderSystem>();

    Entity collidable = registry.CreateEntity();
    collidable.AddComponent<TransformComponent>(glm::vec2(4, 4));
    collidable.AddComponent<BoxColliderComponent>(6, 6);

    Entity transformOnly = registry.CreateEntity();
    transformOnly.AddComponent<TransformComponent>(glm::vec2(30, 30));

    Entity colliderOnly = registry.CreateEntity();
    colliderOnly.AddComponent<BoxColliderComponent>(6, 6);

    registry.Update();

    auto &entities =
        registry.GetSystem<RenderColliderSystem>().GetSystemEntities();
    // Both transform carriers are members; the collider-only entity is not.
    Assert::That(entities.size(), Equals(2u));
    Assert::That(entities[0].GetId(), Equals(collidable.GetId()));
    Assert::That(entities[1].GetId(), Equals(transformOnly.GetId()));

    registry.GetSystem<RenderColliderSystem>().Update(target.renderer);

    // Only the one with a collider left a mark.
    Assert::That(target.RgbAt(4, 4), Equals(kGreen));
    Assert::That(target.DrawnPixelCount(), Equals(2 * (6 + 6) - 4));
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

  // The camera is optional, so an Update(renderer) call draws in raw world
  // coordinates. Unlike RenderSystem there is no isFixed equivalent: a collider
  // is a body in the world, so when a camera IS passed every outline pans.
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

  It(should_outline_a_circle_collider_around_its_centre) {
    SpecSurfaceTarget target(48, 48);
    Assert::That(target.IsUsable(), Equals(true));

    Registry registry;
    registry.AddSystem<RenderColliderSystem>();

    Entity entity = registry.CreateEntity();
    entity.AddComponent<TransformComponent>(glm::vec2(24, 24));
    entity.AddComponent<CircleColliderComponent>(5.0f);

    registry.Update();
    registry.GetSystem<RenderColliderSystem>().Update(target.renderer);

    Assert::That(target.RgbAt(29, 24), Equals(kGreen)); // due east
    Assert::That(target.RgbAt(19, 24), Equals(kGreen)); // due west
    Assert::That(target.RgbAt(24, 29), Equals(kGreen)); // due south
    Assert::That(target.RgbAt(24, 19), Equals(kGreen)); // due north

    // An outline, not a disc.
    Assert::That(target.RgbAt(24, 24), Equals(SpecSurfaceTarget::kNothing));
    Assert::That(target.RgbAt(26, 24), Equals(SpecSurfaceTarget::kNothing));
    // The corner of the bounding box a circle deliberately does not reach.
    Assert::That(target.RgbAt(29, 29), Equals(SpecSurfaceTarget::kNothing));

    Assert::That(EveryDrawnPixelIsOnTheCircle(target, 24, 24, 5.0f),
                 Equals(true));
  };

  // A circle collider's offset places the CENTRE, unlike a box's, which places
  // the top-left corner. The overlay has to honour that or it draws every
  // circle a radius up and to the left of the body being swept.
  It(should_treat_the_circle_offset_as_the_centre_not_a_corner) {
    SpecSurfaceTarget target(48, 48);
    Assert::That(target.IsUsable(), Equals(true));

    Registry registry;
    registry.AddSystem<RenderColliderSystem>();

    Entity entity = registry.CreateEntity();
    entity.AddComponent<TransformComponent>(glm::vec2(10, 10));
    entity.AddComponent<CircleColliderComponent>(6.0f, glm::vec2(14, 12));

    registry.Update();
    registry.GetSystem<RenderColliderSystem>().Update(target.renderer);

    // Centre at (10 + 14, 10 + 12) = (24, 22).
    Assert::That(target.RgbAt(30, 22), Equals(kGreen));
    Assert::That(target.RgbAt(18, 22), Equals(kGreen));
    Assert::That(EveryDrawnPixelIsOnTheCircle(target, 24, 22, 6.0f),
                 Equals(true));
  };

  // Same rule as ContactSystem::CircleOf: the larger absolute axis wins, so the
  // outline is never quietly smaller than the body, and a mirrored sprite keeps
  // the collider it had instead of inverting its radius.
  It(should_scale_the_circle_by_the_larger_absolute_axis) {
    SpecSurfaceTarget target(48, 48);
    Assert::That(target.IsUsable(), Equals(true));

    Registry registry;
    registry.AddSystem<RenderColliderSystem>();

    Entity entity = registry.CreateEntity();
    entity.AddComponent<TransformComponent>(glm::vec2(24, 24),
                                            glm::vec2(-1, 3));
    entity.AddComponent<CircleColliderComponent>(4.0f);

    registry.Update();
    registry.GetSystem<RenderColliderSystem>().Update(target.renderer);

    // 4 * 3, not 4 * 1 and not 4 * -1.
    Assert::That(target.RgbAt(36, 24), Equals(kGreen));
    Assert::That(EveryDrawnPixelIsOnTheCircle(target, 24, 24, 12.0f),
                 Equals(true));
  };

  // The overlay resolves shapes through ContactSystem's own statics, so this
  // cannot drift from the sweep's box-wins rule: an entity carrying both is a
  // game bug, and the overlay has to show the shape actually being swept.
  It(should_draw_the_box_when_an_entity_carries_both_colliders) {
    SpecSurfaceTarget target(48, 48);
    Assert::That(target.IsUsable(), Equals(true));

    Registry registry;
    registry.AddSystem<RenderColliderSystem>();

    Entity entity = registry.CreateEntity();
    entity.AddComponent<TransformComponent>(glm::vec2(4, 6));
    entity.AddComponent<BoxColliderComponent>(10, 8);
    entity.AddComponent<CircleColliderComponent>(20.0f, glm::vec2(24, 24));

    registry.Update();
    registry.GetSystem<RenderColliderSystem>().Update(target.renderer);

    Assert::That(target.RgbAt(4, 6), Equals(kGreen));
    Assert::That(target.RgbAt(13, 13), Equals(kGreen));
    // The circle would have reached here; the box does not.
    Assert::That(target.RgbAt(44, 24), Equals(SpecSurfaceTarget::kNothing));
    Assert::That(target.DrawnPixelCount(), Equals(2 * (10 + 8) - 4));
  };

  // A radius under one whole pixel -- or a negative one, which is nonsense the
  // solvers clamp to a point -- still marks where the body is.
  It(should_mark_the_centre_when_the_radius_truncates_to_nothing) {
    SpecSurfaceTarget target(48, 48);
    Assert::That(target.IsUsable(), Equals(true));

    Registry registry;
    registry.AddSystem<RenderColliderSystem>();

    Entity entity = registry.CreateEntity();
    entity.AddComponent<TransformComponent>(glm::vec2(12, 20));
    entity.AddComponent<CircleColliderComponent>(0.4f);

    registry.Update();
    registry.GetSystem<RenderColliderSystem>().Update(target.renderer);

    Assert::That(target.RgbAt(12, 20), Equals(kGreen));
    Assert::That(target.DrawnPixelCount(), Equals(1));
  };

  // The other half of requiring the transform alone: the collider is re-read
  // every frame, so one added to a live entity is drawn from then on. Every
  // other system still freezes its component set at admission.
  It(should_draw_a_collider_added_after_the_entity_was_admitted) {
    SpecSurfaceTarget target(48, 48);
    Assert::That(target.IsUsable(), Equals(true));

    Registry registry;
    registry.AddSystem<RenderColliderSystem>();

    Entity entity = registry.CreateEntity();
    entity.AddComponent<TransformComponent>(glm::vec2(24, 24));
    registry.Update();

    auto &system = registry.GetSystem<RenderColliderSystem>();
    system.Update(target.renderer);
    Assert::That(target.DrawnPixelCount(), Equals(0));

    entity.AddComponent<CircleColliderComponent>(5.0f);
    registry.Update();

    system.Update(target.renderer);
    Assert::That(target.RgbAt(29, 24), Equals(kGreen));
    Assert::That(EveryDrawnPixelIsOnTheCircle(target, 24, 24, 5.0f),
                 Equals(true));
  };

  // The overlay takes the same camera RenderSystem does, and pans everything:
  // a collider is a body in the world, so there is no isFixed equivalent to opt
  // one out. Without this a panned game drew its outlines at the raw world
  // position, nowhere near the sprites they belong to.
  It(should_pan_a_box_outline_by_the_camera) {
    SpecSurfaceTarget target(48, 48);
    Assert::That(target.IsUsable(), Equals(true));

    Registry registry;
    registry.AddSystem<RenderColliderSystem>();

    Entity entity = registry.CreateEntity();
    entity.AddComponent<TransformComponent>(glm::vec2(30, 26));
    entity.AddComponent<BoxColliderComponent>(6, 4);

    registry.Update();
    SDL_Rect camera = {20, 20, 48, 48};
    registry.GetSystem<RenderColliderSystem>().Update(target.renderer, &camera);

    // (30 - 20, 26 - 20) = (10, 6).
    Assert::That(target.RgbAt(10, 6), Equals(kGreen));
    Assert::That(target.RgbAt(15, 9), Equals(kGreen));
    Assert::That(target.RgbAt(30, 26), Equals(SpecSurfaceTarget::kNothing));
    Assert::That(target.DrawnPixelCount(), Equals(2 * (6 + 4) - 4));
  };

  It(should_pan_a_circle_outline_by_the_camera) {
    SpecSurfaceTarget target(48, 48);
    Assert::That(target.IsUsable(), Equals(true));

    Registry registry;
    registry.AddSystem<RenderColliderSystem>();

    Entity entity = registry.CreateEntity();
    entity.AddComponent<TransformComponent>(glm::vec2(44, 40));
    entity.AddComponent<CircleColliderComponent>(5.0f);

    registry.Update();
    SDL_Rect camera = {20, 16, 48, 48};
    registry.GetSystem<RenderColliderSystem>().Update(target.renderer, &camera);

    // Centre lands at (44 - 20, 40 - 16) = (24, 24).
    Assert::That(target.RgbAt(29, 24), Equals(kGreen));
    Assert::That(EveryDrawnPixelIsOnTheCircle(target, 24, 24, 5.0f),
                 Equals(true));
  };

  // The camera is a pointer defaulting to nullptr, so every existing
  // Update(renderer) call keeps drawing in raw world coordinates.
  It(should_draw_in_world_space_when_no_camera_is_passed) {
    SpecSurfaceTarget target(48, 48);
    Assert::That(target.IsUsable(), Equals(true));

    Registry registry;
    registry.AddSystem<RenderColliderSystem>();

    Entity entity = registry.CreateEntity();
    entity.AddComponent<TransformComponent>(glm::vec2(30, 26));
    entity.AddComponent<BoxColliderComponent>(6, 4);

    registry.Update();
    registry.GetSystem<RenderColliderSystem>().Update(target.renderer, nullptr);

    Assert::That(target.RgbAt(30, 26), Equals(kGreen));
  };

  // static_cast<int> of a NaN or of a float past INT_MAX is undefined
  // behaviour, and the rasteriser costs one loop iteration per pixel of radius,
  // so an absurd radius is a per-frame hang rather than a wrong picture. Both
  // come from a transform.scale bug, and the overlay's job is to help find that
  // rather than to take the process down with it.
  It(should_draw_nothing_for_a_radius_that_is_not_a_number) {
    SpecSurfaceTarget target(48, 48);
    Assert::That(target.IsUsable(), Equals(true));

    Registry registry;
    registry.AddSystem<RenderColliderSystem>();

    Entity entity = registry.CreateEntity();
    entity.AddComponent<TransformComponent>(glm::vec2(24, 24));
    entity.AddComponent<CircleColliderComponent>(
        std::numeric_limits<float>::quiet_NaN());

    registry.Update();
    registry.GetSystem<RenderColliderSystem>().Update(target.renderer);

    Assert::That(target.DrawnPixelCount(), Equals(0));
  };

  It(should_draw_nothing_for_a_radius_past_any_window) {
    SpecSurfaceTarget target(48, 48);
    Assert::That(target.IsUsable(), Equals(true));

    Registry registry;
    registry.AddSystem<RenderColliderSystem>();

    Entity entity = registry.CreateEntity();
    entity.AddComponent<TransformComponent>(glm::vec2(24, 24));
    entity.AddComponent<CircleColliderComponent>(1.0e9f);

    registry.Update();
    registry.GetSystem<RenderColliderSystem>().Update(target.renderer);

    Assert::That(target.DrawnPixelCount(), Equals(0));
  };

  It(should_draw_nothing_for_a_position_that_is_not_a_number) {
    SpecSurfaceTarget target(48, 48);
    Assert::That(target.IsUsable(), Equals(true));

    Registry registry;
    registry.AddSystem<RenderColliderSystem>();

    Entity boxed = registry.CreateEntity();
    boxed.AddComponent<TransformComponent>(
        glm::vec2(std::numeric_limits<float>::quiet_NaN(), 4));
    boxed.AddComponent<BoxColliderComponent>(6, 4);

    Entity rounded = registry.CreateEntity();
    rounded.AddComponent<TransformComponent>(
        glm::vec2(8, std::numeric_limits<float>::infinity()));
    rounded.AddComponent<CircleColliderComponent>(4.0f);

    registry.Update();
    registry.GetSystem<RenderColliderSystem>().Update(target.renderer);

    Assert::That(target.DrawnPixelCount(), Equals(0));
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
