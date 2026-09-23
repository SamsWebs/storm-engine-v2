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

  // ── P17 residual: mid-file failures must be loud ─────────────────────────
  // The read loop used to put every header field in the while condition and
  // never check the optional collider/animation tails, so a truncated or
  // non-numeric record either dropped the rest of the file with no diagnostic
  // or pushed a corrupt Tile (hasCollider with zero dimensions). Logger keeps
  // a process-wide static history; TileMapLoader owns its own Logger instance,
  // so the per-instance callbacks cannot be used here.

  int errorsLogged() {
    int n = 0;
    for (const auto &entry : Logger::messages)
      if (entry.type == LOG_ERROR)
        n++;
    return n;
  }

  It(should_report_a_truncated_editor_record_instead_of_stopping_silently) {
    int before = errorsLogged();

    TileMapLoader loader("./specs/assets/tilemaps/editorTruncatedHeader.map",
                         "", 8);

    // The complete first record is kept; the half-read second is not pushed.
    Assert::That(loader.getMap().size(), Equals(1u));
    Assert::That(errorsLogged() > before, Equals(true));
  };

  It(should_not_push_an_editor_record_whose_collider_fields_are_missing) {
    int before = errorsLogged();

    TileMapLoader loader("./specs/assets/tilemaps/editorTruncatedCollider.map",
                         "", 8);

    // colliderFlag=1 with no collider fields: the record is incomplete.
    // Pushing it would yield hasCollider=true and colliderW/H left at zero.
    Assert::That(loader.getMap().size(), Equals(1u));
    Assert::That(errorsLogged() > before, Equals(true));
  };

  It(should_not_push_an_editor_record_whose_animation_fields_are_missing) {
    int before = errorsLogged();

    TileMapLoader loader("./specs/assets/tilemaps/editorTruncatedAnim.map", "",
                         8);

    // animatedFlag=1 with no animation fields: same contract as colliders.
    Assert::That(loader.getMap().size(), Equals(1u));
    Assert::That(errorsLogged() > before, Equals(true));
  };

  It(should_report_a_nonnumeric_field_in_an_editor_record) {
    int before = errorsLogged();

    TileMapLoader loader("./specs/assets/tilemaps/editorNonNumeric.map", "", 8);

    Assert::That(loader.getMap().size(), Equals(1u));
    Assert::That(errorsLogged() > before, Equals(true));
  };

  It(should_stay_quiet_when_an_editor_map_is_complete) {
    int before = errorsLogged();

    TileMapLoader loader(editorMap, "", 8);

    // Guard against the fix becoming trigger-happy: a clean EOF after the
    // last full record is not a truncated record.
    Assert::That(loader.getMap().size(), Equals(4u));
    Assert::That(errorsLogged(), Equals(before));
  };

  It(should_report_tile_width_zero_instead_of_dividing_by_zero) {
    // Both the constructor tileSize and the record's tileW are zero, so the
    // grid-position divide has no divisor. Report rather than trap.
    int before = errorsLogged();

    TileMapLoader loader("./specs/assets/tilemaps/editorZeroTileWidth.map", "",
                         0);

    Assert::That(loader.getMap().size(), Equals(0u));
    Assert::That(errorsLogged() > before, Equals(true));
  };
};
