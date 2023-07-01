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
};
