#include "../common/tilemapLoader.h"
#include <igloo/igloo_alt.h>

using namespace igloo;

Describe(TileMapLoaderSpec) {
  const std::string fileMap = "./specs/assets/tilemaps/jungle.map";
  const std::string filePng = "./specs/assets/tilemaps/jungle.png";

  It(should_load_tile_map_and_image_files) {

    // Arrange
    TileMapLoader tileMapLoader(fileMap, filePng);

    // Act
    const Map &map = tileMapLoader.getMap();
    const glm::ivec2 mapResolution = tileMapLoader.getMapResolution();

    // Assert
    Assert::That(map.size(), Equals(500));
    Assert::That(mapResolution.x, Equals(320));
    Assert::That(mapResolution.y, Equals(96));
  }

  It(should_return_correct_pixel_position_from_tile_position) {
    // Arrange
    TileMapLoader tileMapLoader(fileMap, filePng);

    // Act
    glm::ivec2 pixelPos = tileMapLoader.pixelPosFromTilePos(2);

    // Assert
    Assert::That(pixelPos.x, Equals(64));
    Assert::That(pixelPos.y, Equals(0));
  }

  // ── P17: failures must be loud, and must not crash ─────────────────────────
  // Each case below used to produce an empty map with no diagnostic at all, or
  // divide by zero. An empty map is indistinguishable from a successful load of
  // a file with no tiles, so the failure has to reach the log. Logger keeps a
  // process-wide static history; the per-instance callbacks cannot be used here
  // because TileMapLoader owns its own Logger.

  int errorsLogged() {
    int n = 0;
    for (const auto &entry : Logger::messages)
      if (entry.type == LOG_ERROR)
        n++;
    return n;
  }

  It(should_report_a_missing_map_file_rather_than_loading_nothing_silently) {
    int before = errorsLogged();

    TileMapLoader loader("./specs/assets/tilemaps/does-not-exist.map", filePng);

    Assert::That(loader.getMap().size(), Equals(0u));
    Assert::That(errorsLogged() > before, Equals(true));
  }

  It(should_not_divide_by_zero_when_a_csv_map_has_no_tileset) {
    // The CSV path indexes into the tileset, so with no PNG there is no grid
    // width. mapResolution was uninitialised here and the division used it.
    int before = errorsLogged();

    TileMapLoader loader(fileMap, "");

    Assert::That(loader.getMapResolution().x, Equals(0));
    Assert::That(loader.getMap().size(), Equals(0u));
    Assert::That(errorsLogged() > before, Equals(true));
  }

  It(should_return_a_zero_rect_rather_than_dividing_by_zero) {
    TileMapLoader loader(fileMap, "");

    glm::ivec2 pos = loader.pixelPosFromTilePos(7);

    Assert::That(pos.x, Equals(0));
    Assert::That(pos.y, Equals(0));
  }

  It(should_skip_a_malformed_csv_cell_instead_of_throwing) {
    // std::stoi threw here, and the Switch build compiles -fno-exceptions,
    // where that aborts the process rather than raising.
    int before = errorsLogged();

    TileMapLoader loader("./specs/assets/tilemaps/malformed.map", filePng);

    // 8 cells, one unparseable: the other 7 still load.
    Assert::That(loader.getMap().size(), Equals(7u));
    Assert::That(errorsLogged() > before, Equals(true));
  }
};
