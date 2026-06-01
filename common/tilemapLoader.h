#pragma once

#include "SDL2/SDL_image.h"
#include "glm/glm.hpp"
#include "logger.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

struct Tile {
  glm::ivec2 relativePosition;   // (col, row) in tile units
  glm::ivec2 pixelSrcPosition;   // (srcX, srcY) in the tileset PNG
  glm::vec2  scale       = {1.0f, 1.0f};
  int        zIndex      = 0;
  std::string assetId    = "";
  bool        hasCollider = false;
  int         colliderW  = 0;
  int         colliderH  = 0;
};

using Map = std::vector<Tile>;

class TileMapLoader {
public:
  // Legacy constructor: filePng required for CSV maps; pass "" for editor maps.
  explicit TileMapLoader(const std::string &fileMap,
                         const std::string &filePng = "",
                         int tileSize = 32);

  glm::ivec2 pixelPosFromTilePos(int tilePos);
  ~TileMapLoader();
  const Map &getMap() const;
  const glm::ivec2 getMapResolution() const;

private:
  // Returns true if the map was written by the editor (space-separated).
  bool isEditorFormat(const std::string &fileMap);

  void loadFilemapCSV(const std::string &fileMap);
  void loadFilemapEditor(const std::string &fileMap);
  void loadImg(const std::string &filePng);

  int        tileSize;
  Map        map;
  glm::ivec2 mapResolution;
  SDL_Surface *mapSurface;
  Logger     logger;
};
