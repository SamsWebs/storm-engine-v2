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

  // Everything below is written by the tile editor and, before 2.0.0, parsed
  // by TileMapLoader and thrown away because Tile had nowhere to put it. The
  // editor's animation UI therefore had no effect at runtime and animated
  // tiles rendered as static ones.
  //
  // These are appended rather than grouped with the fields they belong beside,
  // which costs 8 bytes of padding. That is deliberate: a game writing
  // Tile{pos, src, scale, z, id, true, 32, 32} positionally still assigns the
  // same eight fields. Reordering would have silently shifted `true` onto
  // colliderW -- a bool converts to int without a diagnostic, so the mistake
  // would compile and misbehave.

  // Collider offset in pixels. Written by the editor since colliders were
  // added; the loader read it off the line and dropped it.
  glm::vec2 colliderOffset = {0.0f, 0.0f};

  bool isAnimated = false;
  // Named to match AnimationComponent, so building one is a direct copy:
  //   AnimationComponent(t.numFrames, t.frameSpeedRate, t.vertical,
  //                      t.isLooped, t.frameOffset)
  // The engine does not build it for you -- TileMapLoader hands back a Map and
  // nothing else; spawning entities is the game's job.
  int numFrames = 1;
  int frameSpeedRate = 1;
  // Note the direction: `vertical` true walks the sprite sheet down a column.
  // The editor writes whichever the tile was authored with; a tile animated
  // along a row is `vertical = false`. Getting this backwards draws nothing,
  // because the source rect walks off the sheet.
  bool vertical = true;
  bool isLooped = true;
  int frameOffset = 0;
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
