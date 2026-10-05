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
// The optional collider and animation tails are checked field-by-field, and a
// record that claims one and then ends early is reported rather than pushed
// with its dimensions left at zero. A clean EOF after a complete record is
// still a normal end of file, not an error -- the last record may legitimately
// omit its animation flag. All of that moved to TileRecordReader; see the note
// at its call site below.
void TileMapLoader::loadFilemapEditor(const std::string &fileMap) {
  std::ifstream fmap{fileMap};
  if (!fmap.is_open()) {
    logger.Err("TileMapLoader: cannot open " + fileMap);
    return;
  }

  // The version header, read from THIS stream so the records that follow start
  // after it — reading it from a second open of the same file leaves the parser
  // looking at "storm-map" as the first group name, and every versioned map
  // loads zero tiles.
  //
  // A map written by a newer engine is refused here rather than half-read:
  // half-reading produces a level that renders and is wrong, which is the worst
  // outcome available, because nothing about it looks broken. A map with NO
  // header is version 1 and loads unchanged, which is what keeps every map
  // written before the header existed working.
  const TileMapVersion version = ReadTileMapVersion(fmap);
  const std::string refusal = TileMapVersionRefusal(version, fileMap);
  if (!refusal.empty()) {
    logger.Err(refusal);
    return;
  }

  // The record parsing is NOT here any more. It lives in
  // <stormengine2/tilemapFormat.h> as TileRecordReader, because it used to be
  // written by hand TWICE -- once here and once in the editor's FileLoader --
  // and the editor's copy is a separate binary that ships separately, which is
  // how the two drifted.
  //
  // TileRecordReader is also the only thing here that knows about the optional
  // animation flag and the one-token pushback that separates "this record
  // omitted its flag" from "the file ended". Both are spec'd there; neither was
  // observable here.
  TileRecordReader reader(fmap);

  TileRecord record;
  while (reader.Read(&record)) {
    const int tileW = record.tileWidth;
    const std::string &assetId = record.assetId;

    // Use the tile size passed to the constructor to derive grid position.
    // If zero (not set), fall back to the tile width from the map line.
    int ts = (tileSize > 0) ? tileSize : tileW;
    if (ts == 0) {
      logger.Err("TileMapLoader: '" + fileMap + "': record '" + assetId +
                 "' has tile width 0 and no constructor tileSize; stopping");
      return;
    }

    Tile tile;
    tile.relativePosition = glm::ivec2(static_cast<int>(record.worldX) / ts,
                                       static_cast<int>(record.worldY) / ts);
    tile.pixelSrcPosition = glm::ivec2(record.srcRectX, record.srcRectY);
    tile.scale = glm::vec2(record.scaleX, record.scaleY);
    tile.zIndex = record.zIndex;
    tile.assetId = record.assetId;
    tile.hasCollider = record.collider;
    tile.colliderW = record.colliderWidth;
    tile.colliderH = record.colliderHeight;
    tile.colliderOffset =
        glm::vec2(record.colliderOffsetX, record.colliderOffsetY);
    tile.isAnimated = record.animated;
    tile.numFrames = record.numFrames;
    tile.frameSpeedRate = record.frameSpeed;
    tile.vertical = record.vertical;
    tile.isLooped = record.looped;
    tile.frameOffset = record.frameOffset;

    map.push_back(tile);
  }

  // The reader distinguishes "the map ended" from "the map is damaged", which
  // this loop cannot do by itself: a failed Read is both. Truncation used to
  // be reported per-field from three separate places; it is now one report,
  // and it names the record that was damaged rather than only the field.
  if (reader.Truncated()) {
    logger.Err("TileMapLoader: '" + fileMap + "': truncated or malformed " +
               reader.TruncatedWhat() + " after " + std::to_string(map.size()) +
               " complete tiles; stopping");
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
