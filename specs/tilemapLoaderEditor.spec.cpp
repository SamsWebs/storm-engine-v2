#include "../common/tilemapLoader.h"
#include <igloo/igloo_alt.h>

using namespace igloo;
using namespace storm;

// Editor-format maps are space-separated and carry full per-tile metadata
// (world position, scale, colliders, animation). No PNG is needed — the world
// coordinates are divided by the tileSize to recover the grid position.
static const std::string editorMap = "./specs/assets/tilemaps/editor.map";

Describe(TileMapLoaderEditorSpec) {

  It(should_load_every_tile_in_the_file) {
    TileMapLoader loader(editorMap, "", 8);
    Assert::That(loader.getMap().size(), Equals(4u));
  };

  It(should_derive_grid_position_by_dividing_world_coords_by_tile_size) {
    TileMapLoader loader(editorMap, "", 8);
    const Map &map = loader.getMap();
    // grass at world (0,0)
    Assert::That(map[0].relativePosition.x, Equals(0));
    Assert::That(map[0].relativePosition.y, Equals(0));
    // wall at world (16,24) with tileSize 8 -> (2,3)
    Assert::That(map[1].relativePosition.x, Equals(2));
    Assert::That(map[1].relativePosition.y, Equals(3));
    // water at world (8,8) -> (1,1)
    Assert::That(map[2].relativePosition.x, Equals(1));
    Assert::That(map[2].relativePosition.y, Equals(1));
  };

  It(should_recompute_grid_position_for_a_different_tile_size) {
    TileMapLoader loader(editorMap, "", 16);
    const Map &map = loader.getMap();
    // wall at world (16,24) with tileSize 16 -> (1,1)
    Assert::That(map[1].relativePosition.x, Equals(1));
    Assert::That(map[1].relativePosition.y, Equals(1));
    // water at world (8,8) with tileSize 16 -> (0,0)
    Assert::That(map[2].relativePosition.x, Equals(0));
    Assert::That(map[2].relativePosition.y, Equals(0));
  };

  It(should_read_the_pixel_source_position_from_the_tileset) {
    TileMapLoader loader(editorMap, "", 8);
    const Map &map = loader.getMap();
    Assert::That(map[1].pixelSrcPosition.x, Equals(16));
    Assert::That(map[1].pixelSrcPosition.y, Equals(0));
    Assert::That(map[2].pixelSrcPosition.x, Equals(0));
    Assert::That(map[2].pixelSrcPosition.y, Equals(16));
  };

  It(should_read_asset_id_and_zindex) {
    TileMapLoader loader(editorMap, "", 8);
    const Map &map = loader.getMap();
    Assert::That(map[0].assetId, Equals("grass"));
    Assert::That(map[1].assetId, Equals("wall"));
    Assert::That(map[1].zIndex, Equals(1));
  };

  It(should_read_per_tile_scale) {
    TileMapLoader loader(editorMap, "", 8);
    const Map &map = loader.getMap();
    Assert::That(map[2].scale.x, Equals(2.f));
    Assert::That(map[2].scale.y, Equals(3.f));
  };

  It(should_flag_tiles_with_a_collider_and_read_its_size) {
    TileMapLoader loader(editorMap, "", 8);
    const Map &map = loader.getMap();
    Assert::That(map[1].hasCollider, Equals(true));
    Assert::That(map[1].colliderW, Equals(8));
    Assert::That(map[1].colliderH, Equals(8));
  };

  It(should_leave_collider_off_for_tiles_without_one) {
    TileMapLoader loader(editorMap, "", 8);
    const Map &map = loader.getMap();
    Assert::That(map[0].hasCollider, Equals(false));
    Assert::That(map[2].hasCollider, Equals(false));
  };

  // Before 2.0.0 the collider offset was read off the line only to advance the
  // stream, then dropped -- so a tile whose collider the editor had nudged
  // collided from its unnudged position.
  It(should_read_the_collider_offset) {
    TileMapLoader loader(editorMap, "", 8);
    const Map &map = loader.getMap();
    Assert::That(map[1].colliderOffset.x, Equals(2.f));
    Assert::That(map[1].colliderOffset.y, Equals(3.f));
  };

  // Same story for all five animation fields: the editor wrote them, the
  // loader parsed and discarded them, so animated tiles rendered static.
  // `water` is written as 4 frames at speed 10, along a row (vertical 0),
  // looped, offset 0.
  It(should_read_every_animation_field) {
    TileMapLoader loader(editorMap, "", 8);
    const Map &map = loader.getMap();
    Assert::That(map[2].isAnimated, Equals(true));
    Assert::That(map[2].numFrames, Equals(4));
    Assert::That(map[2].frameSpeedRate, Equals(10));
    Assert::That(map[2].vertical, Equals(false));
    Assert::That(map[2].isLooped, Equals(true));
    Assert::That(map[2].frameOffset, Equals(0));
  };

  // A tile with no animation must not inherit the previous tile's values --
  // the parse variables are reused across loop iterations, so this is the case
  // that catches them being left over rather than reset.
  It(should_leave_animation_defaulted_for_tiles_without_one) {
    TileMapLoader loader(editorMap, "", 8);
    const Map &map = loader.getMap();
    Assert::That(map[0].isAnimated, Equals(false));
    Assert::That(map[0].numFrames, Equals(1));
    Assert::That(map[0].frameSpeedRate, Equals(1));
    Assert::That(map[0].isLooped, Equals(true));
    Assert::That(map[0].frameOffset, Equals(0));

    // map[3] is the load-bearing one. It is the plain tile written *after*
    // the animated map[2], so it is the only position where animation state
    // leaking across loop iterations is observable -- the parse variables are
    // reused, and a declaration hoisted out of the loop would carry water's
    // 4 frames at speed 10 straight into it. Asserting this on map[0] or
    // map[1] would pass no matter what, since nothing animated precedes them.
    Assert::That(map[3].isAnimated, Equals(false));
    Assert::That(map[3].numFrames, Equals(1));
    Assert::That(map[3].frameSpeedRate, Equals(1));
    Assert::That(map[3].vertical, Equals(true));
    Assert::That(map[3].isLooped, Equals(true));
    Assert::That(map[3].frameOffset, Equals(0));

    // And the collider offset leaks the same way: map[1] has one, map[3]
    // must not inherit it.
    Assert::That(map[3].colliderOffset.x, Equals(0.f));
    Assert::That(map[3].colliderOffset.y, Equals(0.f));
  };

  // Colliders and animation are independent flags on the same line, and the
  // collider block is optional -- so a tile with a collider and no animation
  // must not read the animation flag out of the collider's fields.
  It(should_keep_collider_and_animation_independent) {
    TileMapLoader loader(editorMap, "", 8);
    const Map &map = loader.getMap();
    Assert::That(map[1].hasCollider, Equals(true));
    Assert::That(map[1].isAnimated, Equals(false));
    Assert::That(map[2].hasCollider, Equals(false));
    Assert::That(map[2].isAnimated, Equals(true));
  };
};
