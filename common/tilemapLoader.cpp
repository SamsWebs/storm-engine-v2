#include "tilemapLoader.h"

TileMapLoader::TileMapLoader(const std::string &fileMap,
                             const std::string &filePng, int tileSize)
    : tileSize{tileSize}, mapSurface{nullptr} {
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
  std::string line;
  int y = 0;
  if (fmap.is_open()) {
    while (fmap >> line) {
      std::stringstream s{line};
      std::string strNum;
      int x = 0;
      while (std::getline(s, strNum, ',')) {
        Tile tile;
        tile.relativePosition = glm::ivec2(x, y);
        tile.pixelSrcPosition = pixelPosFromTilePos(std::stoi(strNum));
        map.push_back(tile);
        x++;
      }
      y++;
    }
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// Editor format written by FileLoader::SaveMap:
//
//   group assetId tileW tileH srcX srcY zIndex worldX worldY scaleX scaleY
//   collider [colW colH offX offY] animated [numFrames speed vert loop frameOff]
//
void TileMapLoader::loadFilemapEditor(const std::string &fileMap) {
  std::ifstream fmap{fileMap};
  if (!fmap.is_open()) {
    logger.Err("TileMapLoader: cannot open " + fileMap);
    return;
  }

  std::string group, assetId;
  int tileW, tileH, srcX, srcY, zIndex;
  float worldX, worldY, scaleX, scaleY;
  int colliderFlag, animatedFlag;

  while (fmap >> group >> assetId >> tileW >> tileH >> srcX >> srcY >>
         zIndex >> worldX >> worldY >> scaleX >> scaleY >> colliderFlag) {

    int colW = 0, colH = 0;
    float offX = 0.0f, offY = 0.0f;
    if (colliderFlag)
      fmap >> colW >> colH >> offX >> offY;

    if (!(fmap >> animatedFlag))
      animatedFlag = 0;

    // Consume optional animation fields if present
    if (animatedFlag) {
      int numFrames, frameSpeed, frameOffset;
      bool vertical, looped;
      fmap >> numFrames >> frameSpeed >> vertical >> looped >> frameOffset;
    }

    // Use the tile size passed to the constructor to derive grid position.
    // If zero (not set), fall back to the tile width from the map line.
    int ts = (tileSize > 0) ? tileSize : tileW;

    Tile tile;
    tile.relativePosition = glm::ivec2(static_cast<int>(worldX) / ts,
                                       static_cast<int>(worldY) / ts);
    tile.pixelSrcPosition = glm::ivec2(srcX, srcY);
    tile.scale            = glm::vec2(scaleX, scaleY);
    tile.zIndex           = zIndex;
    tile.assetId          = assetId;
    tile.hasCollider      = (colliderFlag != 0);
    tile.colliderW        = colW;
    tile.colliderH        = colH;

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
  const int tilesCountHorizontal = mapResolution.x / tileSize;
  const int row    = tilePos / tilesCountHorizontal;
  const int column = tilePos % tilesCountHorizontal;
  return glm::ivec2(column * tileSize, row * tileSize);
}

const Map &TileMapLoader::getMap() const { return map; }

const glm::ivec2 TileMapLoader::getMapResolution() const {
  return mapResolution;
}
