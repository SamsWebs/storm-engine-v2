#include "tilemapLoader.h"

namespace storm {

TileMapLoader::TileMapLoader(const std::string &fileMap,
                             const std::string &filePng, int tileSize)
    : tileSize{tileSize}, mapSurface{nullptr} {
  // A file that cannot be opened used to fall through to the CSV branch, where
  // it produced an empty map and no diagnostic at all — a missing map and a
  // map with no tiles were indistinguishable, and the empty result surfaced
  // later as a crash somewhere unrelated. Report it here, once.
  {
    std::ifstream probe{fileMap};
    if (!probe.is_open()) {
      logger.Err("TileMapLoader: cannot open map file '" + fileMap +
                 "' (check the path is relative to the working directory); "
                 "loading nothing");
      return;
    }
  }

  if (isEditorFormat(fileMap)) {
    loadFilemapEditor(fileMap);
  } else {
    loadImg(filePng);
    loadFilemapCSV(fileMap);
  }
}

TileMapLoader::~TileMapLoader() {
  if (mapSurface)
    SDL_FreeSurface(mapSurface);
  mapSurface = nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
// Detect format: peek at the first token. Editor format starts with a letter
// (the group name, e.g. "tiles"); CSV format starts with a digit.
bool TileMapLoader::isEditorFormat(const std::string &fileMap) {
  std::ifstream f{fileMap};
  if (!f.is_open())
    return false;
  char c = '\0';
  while (f.get(c) && std::isspace(static_cast<unsigned char>(c)))
    ;
  return std::isalpha(static_cast<unsigned char>(c));
}

// ─────────────────────────────────────────────────────────────────────────────
// Legacy CSV format: each row is comma-separated tile indices.
void TileMapLoader::loadFilemapCSV(const std::string &fileMap) {
  std::ifstream fmap{fileMap};
  if (!fmap.is_open()) {
    logger.Err("TileMapLoader: cannot open CSV map '" + fileMap + "'");
    return;
  }

  // CSV maps index into the tileset PNG, so without a loaded PNG there is no
  // grid width to map an index onto. That used to divide by zero.
  if (mapResolution.x <= 0 || tileSize <= 0) {
    logger.Err("TileMapLoader: CSV map '" + fileMap +
               "' needs a tileset PNG and a positive tile size; "
               "pass the PNG path as the second constructor argument");
    return;
  }

  std::string line;
  int y = 0;
  while (fmap >> line) {
    std::stringstream s{line};
    std::string strNum;
    int x = 0;
    while (std::getline(s, strNum, ',')) {
      // strtol, not stoi: stoi throws on a malformed cell, and the Switch
      // build compiles -fno-exceptions where that aborts the process.
      char *end = nullptr;
      const long index = std::strtol(strNum.c_str(), &end, 10);
      if (end == strNum.c_str() || *end != '\0') {
        logger.Err("TileMapLoader: '" + fileMap + "' row " + std::to_string(y) +
                   " column " + std::to_string(x) + " is not a number ('" +
                   strNum + "'); skipping the cell");
        x++;
        continue;
      }

      Tile tile;
      tile.relativePosition = glm::ivec2(x, y);
      tile.pixelSrcPosition = pixelPosFromTilePos(static_cast<int>(index));
      map.push_back(tile);
      x++;
    }
    y++;
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// Editor format written by FileLoader::SaveMap:
//
//   group assetId tileW tileH srcX srcY zIndex worldX worldY scaleX scaleY
//   collider [colW colH offX offY] animated [numFrames speed vert loop
//   frameOff]
//
// Every field on that line now reaches Tile. Before 2.0.0 the collider offset
// and all five animation fields were parsed purely to advance the stream and
// then dropped, because Tile had nowhere to put them.
//
// The optional collider and animation tails are checked field-by-field. A
// record that claims a collider (or an animation) and then ends early used to
// either drop the rest of the file with no diagnostic or push a Tile with
// hasCollider/isAnimated set and the dimensions left at zero. A clean EOF
// after a complete record is still a normal end of file, not an error: the
// `animatedFlag` read may legitimately fail at EOF for a last record that
// omits it, so that one failure is treated as "not animated".
void TileMapLoader::loadFilemapEditor(const std::string &fileMap) {
  std::ifstream fmap{fileMap};
  if (!fmap.is_open()) {
    logger.Err("TileMapLoader: cannot open " + fileMap);
    return;
  }

  auto reportTruncated = [&](const std::string &what) {
    logger.Err("TileMapLoader: '" + fileMap + "': truncated or malformed " +
               what + " after " + std::to_string(map.size()) +
               " complete tiles; stopping");
  };

  std::string group;
  while (fmap >> group) {
    std::string assetId;
    int tileW = 0, tileH = 0, srcX = 0, srcY = 0, zIndex = 0;
    float worldX = 0.0f, worldY = 0.0f, scaleX = 0.0f, scaleY = 0.0f;
    int colliderFlag = 0;

    if (!(fmap >> assetId >> tileW >> tileH >> srcX >> srcY >> zIndex >>
          worldX >> worldY >> scaleX >> scaleY >> colliderFlag)) {
      reportTruncated("header for group '" + group + "'");
      return;
    }

    int colW = 0, colH = 0;
    float offX = 0.0f, offY = 0.0f;
    if (colliderFlag && !(fmap >> colW >> colH >> offX >> offY)) {
      reportTruncated("collider fields for '" + assetId + "'");
      return;
    }

    int animatedFlag = 0;
    if (!(fmap >> animatedFlag)) {
      if (!fmap.eof()) {
        reportTruncated("animation flag for '" + assetId + "'");
        return;
      }
      // Clean EOF before the optional animation flag: last record omits it.
      animatedFlag = 0;
    }

    int numFrames = 1, frameSpeed = 1, frameOffset = 0;
    bool vertical = true, looped = true;
    if (animatedFlag && !(fmap >> numFrames >> frameSpeed >> vertical >>
                          looped >> frameOffset)) {
      reportTruncated("animation fields for '" + assetId + "'");
      return;
    }

    // Use the tile size passed to the constructor to derive grid position.
    // If zero (not set), fall back to the tile width from the map line.
    int ts = (tileSize > 0) ? tileSize : tileW;
    if (ts == 0) {
      logger.Err("TileMapLoader: '" + fileMap + "': record '" + assetId +
                 "' has tile width 0 and no constructor tileSize; stopping");
      return;
    }

    Tile tile;
    tile.relativePosition = glm::ivec2(static_cast<int>(worldX) / ts,
                                       static_cast<int>(worldY) / ts);
    tile.pixelSrcPosition = glm::ivec2(srcX, srcY);
    tile.scale = glm::vec2(scaleX, scaleY);
    tile.zIndex = zIndex;
    tile.assetId = assetId;
    tile.hasCollider = (colliderFlag != 0);
    tile.colliderW = colW;
    tile.colliderH = colH;
    tile.colliderOffset = glm::vec2(offX, offY);
    tile.isAnimated = (animatedFlag != 0);
    tile.numFrames = numFrames;
    tile.frameSpeedRate = frameSpeed;
    tile.vertical = vertical;
    tile.isLooped = looped;
    tile.frameOffset = frameOffset;

    map.push_back(tile);
  }
}

// ─────────────────────────────────────────────────────────────────────────────
void TileMapLoader::loadImg(const std::string &filePng) {
  if (filePng.empty())
    return;
  mapSurface = IMG_Load(filePng.c_str());
  if (!mapSurface) {
    logger.Err("TileMapLoader: error loading PNG: " + filePng);
    return;
  }
  mapResolution.x = mapSurface->w;
  mapResolution.y = mapSurface->h;
}

glm::ivec2 TileMapLoader::pixelPosFromTilePos(int tilePos) {
  // Guard the divisor rather than trusting the caller: this is public, and a
  // tileset narrower than one tile (or no tileset at all) made it divide by
  // zero, which is a crash rather than a bad coordinate.
  const int tilesCountHorizontal =
      (tileSize > 0) ? (mapResolution.x / tileSize) : 0;
  if (tilesCountHorizontal <= 0) {
    logger.Err("TileMapLoader: no tileset loaded, or it is narrower than one "
               "tile; cannot map tile index " +
               std::to_string(tilePos) + " to a source rect");
    return glm::ivec2(0, 0);
  }
  const int row = tilePos / tilesCountHorizontal;
  const int column = tilePos % tilesCountHorizontal;
  return glm::ivec2(column * tileSize, row * tileSize);
}

const Map &TileMapLoader::getMap() const { return map; }

const glm::ivec2 TileMapLoader::getMapResolution() const {
  return mapResolution;
}

} // namespace storm
