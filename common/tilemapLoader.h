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
  glm::ivec2 relativePosition;
  glm::ivec2 pixelSrcPosition;
};

using Map = std::vector<Tile>;
class TileMapLoader {
public:
  explicit TileMapLoader(const std::string &fileMap, const std::string &filePng,
                         int tileSize = 32);

  glm::ivec2 pixelPosFromTilePos(int tilePos);
  ~TileMapLoader();
  const Map &getMap() const;
  const glm::ivec2 getMapResolution() const;

private:
  void loadFilemap(const std::string &fileMap);
  void loadImg(const ::std::string &filePng);
  int tileSize;
  Map map;
  glm::ivec2 mapResolution;
  SDL_Surface *mapSurface;
  Logger logger;
};