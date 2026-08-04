#pragma once

#include "SDL2/SDL_image.h"
#include "glm/glm.hpp"
#include "logger.h"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

struct Tile {
  glm::ivec2 relativePosition; // (col, row) in tile units
  glm::ivec2 pixelSrcPosition; // (srcX, srcY) in the tileset PNG
  glm::vec2 scale = {1.0f, 1.0f};
  int zIndex = 0;
  std::string assetId = "";
  bool hasCollider = false;
  int colliderW = 0;
  int colliderH = 0;
};

using Map = std::vector<Tile>;

class TileMapLoader {
public:
  // Legacy constructor: filePng required for CSV maps; pass "" for editor maps.
  explicit TileMapLoader(const std::string &fileMap,
                         const std::string &filePng = "", int tileSize = 32);

  glm::ivec2 pixelPosFromTilePos(int tilePos);
  ~TileMapLoader();
  // Empty when the file was missing, unreadable, or contained no tiles. Every
  // failure is reported through Logger::Err first — check this before using a
  // map, since an empty one is indistinguishable from a load that failed.
  const Map &getMap() const;
  const glm::ivec2 getMapResolution() const;

private:
  // Returns true if the map was written by the editor (space-separated).
  bool isEditorFormat(const std::string &fileMap);

  void loadFilemapCSV(const std::string &fileMap);
  void loadFilemapEditor(const std::string &fileMap);
  void loadImg(const std::string &filePng);

  int tileSize;
  Map map;
  // Zero-initialised: on the CSV path with no PNG, loadImg() returns before
  // setting this, and pixelPosFromTilePos() divides by it. Left uninitialised
  // that was a division by garbage.
  glm::ivec2 mapResolution = {0, 0};
  SDL_Surface *mapSurface;
  Logger logger;
};
