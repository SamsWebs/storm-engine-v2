#include "../common/tilemapLoader.h"
#include <igloo/igloo_alt.h>

using namespace igloo;

// Editor-format maps are space-separated and carry full per-tile metadata
// (world position, scale, colliders, animation). No PNG is needed — the world
// coordinates are divided by the tileSize to recover the grid position.
static const std::string editorMap = "./specs/assets/tilemaps/editor.map";

Describe(TileMapLoaderEditorSpec) {

  It(should_load_every_tile_in_the_file) {
    TileMapLoader loader(editorMap, "", 8);
    Assert::That(loader.getMap().size(), Equals(3u));
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
};
