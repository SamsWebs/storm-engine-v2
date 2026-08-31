#include "../../common/systems/render.h"
#include "../support/softwareRenderer.h"
#include <igloo/igloo_alt.h>

using namespace igloo;

namespace {

// The white fixture colour-modded per asset id, so two sprites drawn over one
// another can be told apart in the output.
constexpr Uint32 kRed = 0xFF0000;
constexpr Uint32 kBlue = 0x0000FF;

// Loads the shared white fixture under `assetId` and tints it. Returns the
// texture so the caller can assert the fixture was actually found — a missing
// file would otherwise turn every pixel assertion below into a silent pass
// against a blank surface.
SDL_Texture *AddTintedTexture(AssetStore &assetStore, SDL_Renderer *renderer,
                              const std::string &assetId, Uint8 r, Uint8 g,
                              Uint8 b) {
  assetStore.AddTexture(renderer, assetId, SpecWhiteTexturePath());
  SDL_Texture *texture = assetStore.GetTexture(assetId);
  if (texture != nullptr) {
    SDL_SetTextureColorMod(texture, r, g, b);
  }
  return texture;
}

} // namespace

// NOTE ON DECLARATION ORDER: SpecSurfaceTarget is declared before AssetStore
// in every case below, so the store's destructor frees its textures while the
// renderer that created them is still alive.
Describe(RenderSystemSpec) {
  It(should_select_only_entities_that_have_both_a_transform_and_a_sprite) {
    Registry registry;
    registry.AddSystem<RenderSystem>();

    Entity drawable = registry.CreateEntity();
    drawable.AddComponent<TransformComponent>();
    drawable.AddComponent<SpriteComponent>();

    Entity transformOnly = registry.CreateEntity();
    transformOnly.AddComponent<TransformComponent>();

    Entity spriteOnly = registry.CreateEntity();
    spriteOnly.AddComponent<SpriteComponent>();

    registry.Update();

    auto &entities = registry.GetSystem<RenderSystem>().GetSystemEntities();
    Assert::That(entities.size(), Equals(1u));
    Assert::That(entities[0].GetId(), Equals(drawable.GetId()));
  };

  // sortEntities is protected and reachable only by subclassing; RenderSystem
  // is the engine's one caller, and this ordering is the whole of its
  // contract. The sort runs before any drawing, so it is verifiable
  // independently of what SDL puts on the surface.
  It(should_sort_its_entities_by_ascending_z_index) {
    SpecSurfaceTarget target(32, 32);
    Assert::That(target.IsUsable(), Equals(true));
    AssetStore assetStore;

    Registry registry;
    registry.AddSystem<RenderSystem>();

    // Created highest-first, so the pre-sort order (entities enter a system in
    // ascending id order) is not the expected one.
    Entity high = registry.CreateEntity();
    high.AddComponent<TransformComponent>();
    high.AddComponent<SpriteComponent>("", 4, 4, 5);

    Entity low = registry.CreateEntity();
    low.AddComponent<TransformComponent>();
    low.AddComponent<SpriteComponent>("", 4, 4, -2);

    Entity middle = registry.CreateEntity();
    middle.AddComponent<TransformComponent>();
    middle.AddComponent<SpriteComponent>("", 4, 4, 1);

    registry.Update();

    auto &entities = registry.GetSystem<RenderSystem>().GetSystemEntities();
    Assert::That(entities.size(), Equals(3u));
    Assert::That(entities[0].GetId(), Equals(high.GetId())); // not sorted yet

    registry.GetSystem<RenderSystem>().Update(target.renderer, assetStore);

    Assert::That(entities[0].GetId(), Equals(low.GetId()));
    Assert::That(entities[1].GetId(), Equals(middle.GetId()));
    Assert::That(entities[2].GetId(), Equals(high.GetId()));
  };

  It(should_draw_a_higher_z_index_over_a_lower_one) {
    SpecSurfaceTarget target(32, 32);
    Assert::That(target.IsUsable(), Equals(true));
    AssetStore assetStore;

    Assert::That(AddTintedTexture(assetStore, target.renderer, "red", 0xFF,
                                  0x00, 0x00) != nullptr,
                 Equals(true));
    Assert::That(AddTintedTexture(assetStore, target.renderer, "blue", 0x00,
                                  0x00, 0xFF) != nullptr,
                 Equals(true));

    Registry registry;
    registry.AddSystem<RenderSystem>();

    // Blue is the larger sprite underneath and red the smaller one on top, so
    // a ring of blue survives and proves both were actually drawn.
    //
    // Red is created first on purpose: entities enter a system in ascending
    // id order, so drawing in list order without the z-index sort would put
    // blue on top and this case would catch it.
    Entity red = registry.CreateEntity();
    red.AddComponent<TransformComponent>(glm::vec2(4, 4));
    red.AddComponent<SpriteComponent>("red", 4, 4, 1);

    Entity blue = registry.CreateEntity();
    blue.AddComponent<TransformComponent>(glm::vec2(4, 4));
    blue.AddComponent<SpriteComponent>("blue", 6, 6, 0);

    registry.Update();
    registry.GetSystem<RenderSystem>().Update(target.renderer, assetStore);

    Assert::That(target.RgbAt(4, 4), Equals(kRed));  // both cover it
    Assert::That(target.RgbAt(7, 7), Equals(kRed));  // last red pixel
    Assert::That(target.RgbAt(8, 8), Equals(kBlue)); // blue only
    Assert::That(target.RgbAt(9, 9), Equals(kBlue)); // last blue pixel
    Assert::That(target.RgbAt(10, 10), Equals(SpecSurfaceTarget::kNothing));
  };

  It(should_shift_a_sprite_by_the_camera_origin) {
    SpecSurfaceTarget target(32, 32);
    Assert::That(target.IsUsable(), Equals(true));
    AssetStore assetStore;

    Assert::That(AddTintedTexture(assetStore, target.renderer, "red", 0xFF,
                                  0x00, 0x00) != nullptr,
                 Equals(true));

    Registry registry;
    registry.AddSystem<RenderSystem>();

    Entity entity = registry.CreateEntity();
    entity.AddComponent<TransformComponent>(glm::vec2(20, 20));
    entity.AddComponent<SpriteComponent>("red", 4, 4);

    registry.Update();

    SDL_Rect camera = {5, 5, 32, 32};
    registry.GetSystem<RenderSystem>().Update(target.renderer, assetStore,
                                              &camera);

    Assert::That(target.RgbAt(15, 15), Equals(kRed)); // 20 - 5
    Assert::That(target.RgbAt(18, 18), Equals(kRed)); // 20 + 4 - 5 - 1
    Assert::That(target.RgbAt(19, 19), Equals(SpecSurfaceTarget::kNothing));
    Assert::That(target.RgbAt(20, 20), Equals(SpecSurfaceTarget::kNothing));
  };

  It(should_ignore_the_camera_for_a_fixed_sprite) {
    SpecSurfaceTarget target(32, 32);
    Assert::That(target.IsUsable(), Equals(true));
    AssetStore assetStore;

    Assert::That(AddTintedTexture(assetStore, target.renderer, "red", 0xFF,
                                  0x00, 0x00) != nullptr,
                 Equals(true));

    Registry registry;
    registry.AddSystem<RenderSystem>();

    Entity entity = registry.CreateEntity();
    entity.AddComponent<TransformComponent>(glm::vec2(20, 20));
    entity.AddComponent<SpriteComponent>("red", 4, 4, 0, /*isFixed=*/true);

    registry.Update();

    SDL_Rect camera = {5, 5, 32, 32};
    registry.GetSystem<RenderSystem>().Update(target.renderer, assetStore,
                                              &camera);

    Assert::That(target.RgbAt(20, 20), Equals(kRed)); // screen space, unpanned
    Assert::That(target.RgbAt(15, 15), Equals(SpecSurfaceTarget::kNothing));
  };

  It(should_add_the_sprite_offset_to_the_transform_position) {
    SpecSurfaceTarget target(32, 32);
    Assert::That(target.IsUsable(), Equals(true));
    AssetStore assetStore;

    Assert::That(AddTintedTexture(assetStore, target.renderer, "red", 0xFF,
                                  0x00, 0x00) != nullptr,
                 Equals(true));

    Registry registry;
    registry.AddSystem<RenderSystem>();

    Entity entity = registry.CreateEntity();
    entity.AddComponent<TransformComponent>(glm::vec2(4, 4));
    entity.AddComponent<SpriteComponent>("red", 4, 4, 0, false, 0, 0,
                                         glm::vec2(3, 6));

    registry.Update();
    registry.GetSystem<RenderSystem>().Update(target.renderer, assetStore);

    Assert::That(target.RgbAt(7, 10), Equals(kRed)); // (4 + 3, 4 + 6)
    Assert::That(target.RgbAt(4, 4), Equals(SpecSurfaceTarget::kNothing));
  };

  It(should_scale_the_destination_size_by_the_transform_scale) {
    SpecSurfaceTarget target(32, 32);
    Assert::That(target.IsUsable(), Equals(true));
    AssetStore assetStore;

    Assert::That(AddTintedTexture(assetStore, target.renderer, "red", 0xFF,
                                  0x00, 0x00) != nullptr,
                 Equals(true));

    Registry registry;
    registry.AddSystem<RenderSystem>();

    Entity entity = registry.CreateEntity();
    entity.AddComponent<TransformComponent>(glm::vec2(0, 0), glm::vec2(2, 2));
    entity.AddComponent<SpriteComponent>("red", 4, 4);

    registry.Update();
    registry.GetSystem<RenderSystem>().Update(target.renderer, assetStore);

    Assert::That(target.RgbAt(7, 7), Equals(kRed)); // 4 * 2 = 8 wide
    Assert::That(target.RgbAt(8, 8), Equals(SpecSurfaceTarget::kNothing));
    Assert::That(target.DrawnPixelCount(), Equals(64));
  };

  It(should_draw_nothing_when_no_entity_matches) {
    SpecSurfaceTarget target(32, 32);
    Assert::That(target.IsUsable(), Equals(true));
    AssetStore assetStore;

    Registry registry;
    registry.AddSystem<RenderSystem>();
    registry.Update();

    registry.GetSystem<RenderSystem>().Update(target.renderer, assetStore);

    Assert::That(registry.GetSystem<RenderSystem>().GetSystemEntities().size(),
                 Equals(0u));
    Assert::That(target.DrawnPixelCount(), Equals(0));
  };

  // A sprite naming an asset the store never loaded is a content bug, not a
  // crash: AssetStore::GetTexture answers nullptr and SDL_RenderCopyEx
  // rejects it.
  It(should_draw_nothing_for_a_sprite_whose_asset_is_missing) {
    SpecSurfaceTarget target(32, 32);
    Assert::That(target.IsUsable(), Equals(true));
    AssetStore assetStore;

    Registry registry;
    registry.AddSystem<RenderSystem>();

    Entity entity = registry.CreateEntity();
    entity.AddComponent<TransformComponent>(glm::vec2(4, 4));
    entity.AddComponent<SpriteComponent>("never-loaded", 4, 4);

    registry.Update();
    registry.GetSystem<RenderSystem>().Update(target.renderer, assetStore);

    Assert::That(assetStore.GetTexture("never-loaded") == nullptr,
                 Equals(true));
    Assert::That(target.DrawnPixelCount(), Equals(0));
  };
};

static std::size_t SpecRenderErrorCount() {
  std::size_t errors = 0;
  for (const auto &entry : Logger::messages) {
    if (entry.type == LogType::LOG_ERROR) {
      ++errors;
    }
  }
  return errors;
}

Describe(SrcRectBoundsSpec) {
  It(should_report_a_src_rect_past_the_bottom_of_the_texture) {
    SpecSurfaceTarget target(32, 32);
    AssetStore assetStore;
    SDL_Texture *texture =
        AddTintedTexture(assetStore, target.renderer, "sheet", 255, 255, 255);
    Assert::That(texture == nullptr, Equals(false));

    int textureW = 0, textureH = 0;
    SDL_QueryTexture(texture, nullptr, nullptr, &textureW, &textureH);

    Registry registry;
    registry.AddSystem<RenderSystem>();
    Entity entity = registry.CreateEntity();
    entity.AddComponent<TransformComponent>();
    entity.AddComponent<SpriteComponent>("sheet", textureW, textureH, 0, false,
                                         0, textureH * 4);
    registry.Update();

    Logger::messages.clear();
    registry.GetSystem<RenderSystem>().Update(target.renderer, assetStore);

    Assert::That(SpecRenderErrorCount(),
                 Is().GreaterThanOrEqualTo(static_cast<std::size_t>(1)));
    Logger::messages.clear();
  };

  It(should_stay_silent_for_a_src_rect_inside_the_texture) {
    SpecSurfaceTarget target(32, 32);
    AssetStore assetStore;
    SDL_Texture *texture =
        AddTintedTexture(assetStore, target.renderer, "sheet", 255, 255, 255);
    Assert::That(texture == nullptr, Equals(false));

    int textureW = 0, textureH = 0;
    SDL_QueryTexture(texture, nullptr, nullptr, &textureW, &textureH);

    Registry registry;
    registry.AddSystem<RenderSystem>();
    Entity entity = registry.CreateEntity();
    entity.AddComponent<TransformComponent>();
    entity.AddComponent<SpriteComponent>("sheet", textureW, textureH, 0, false,
                                         0, 0);
    registry.Update();

    Logger::messages.clear();
    registry.GetSystem<RenderSystem>().Update(target.renderer, assetStore);

    Assert::That(SpecRenderErrorCount(), Equals(static_cast<std::size_t>(0)));
  };

  // The srcRect-outside-texture diagnostic's throttle counter
  // (`srcRectReports` in common/systems/render.h) is static thread_local and
  // never reset within a process, so all three cases in this Describe share
  // one budget of ECS_MAX_DIAGNOSTIC_REPORTS (4):
  // should_report_a_src_rect_past_the_bottom_of_the_texture spends 1,
  // should_stay_silent_for_a_src_rect_inside_the_texture spends 0, leaving
  // this case >= 1 of budget no matter what runs first. A bound of exactly 0
  // would pass whether the throttle works or the diagnostic was deleted
  // outright, so this case asserts both that at least one report fires and
  // that the 200-frame hammering never exceeds the shared budget.
  It(should_throttle_the_report_for_a_permanently_broken_sprite) {
    SpecSurfaceTarget target(32, 32);
    AssetStore assetStore;
    SDL_Texture *texture =
        AddTintedTexture(assetStore, target.renderer, "sheet", 255, 255, 255);
    Assert::That(texture == nullptr, Equals(false));

    int textureW = 0, textureH = 0;
    SDL_QueryTexture(texture, nullptr, nullptr, &textureW, &textureH);

    Registry registry;
    registry.AddSystem<RenderSystem>();
    Entity entity = registry.CreateEntity();
    entity.AddComponent<TransformComponent>();
    entity.AddComponent<SpriteComponent>("sheet", textureW, textureH, 0, false,
                                         0, textureH * 4);
    registry.Update();

    Logger::messages.clear();
    for (int frame = 0; frame < 200; ++frame) {
      registry.GetSystem<RenderSystem>().Update(target.renderer, assetStore);
    }

    Assert::That(SpecRenderErrorCount(),
                 Is().GreaterThanOrEqualTo(static_cast<std::size_t>(1)));
    Assert::That(SpecRenderErrorCount(),
                 Is().LessThanOrEqualTo(
                     static_cast<std::size_t>(ECS_MAX_DIAGNOSTIC_REPORTS)));
    Logger::messages.clear();
  };
};
