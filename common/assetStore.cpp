#include "assetStore.h"

namespace storm {

AssetStore::AssetStore() { logger.Log("AssetStore constructor called!"); }

AssetStore::~AssetStore() {
  ClearAssets();
  logger.Log("AssetStore destructor called!");
}

void AssetStore::ClearAssets() {
  for (auto &texture : textures) {
    SDL_DestroyTexture(texture.second);
  }
  textures.clear();

  for (auto &font : fonts) {
    TTF_CloseFont(font.second);
  }
  fonts.clear();

  for (auto &sound : sounds) {
    Mix_FreeChunk(sound.second);
  }
  sounds.clear();
}

void AssetStore::AddTexture(SDL_Renderer *renderer, const std::string &assetId,
                            const std::string &filePath) {
  SDL_Surface *surface = IMG_Load(filePath.c_str());
  if (!surface) {
    logger.Err("AssetStore: failed to load '" + filePath + "' — " +
               std::string(IMG_GetError()));
    return;
  }

  SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
  SDL_FreeSurface(surface);
  if (!texture) {
    logger.Err("AssetStore: failed to create texture for '" + assetId + "' — " +
               std::string(SDL_GetError()));
    return;
  }

  // Re-adding an id replaces (and frees) the old texture instead of
  // silently leaking the new one.
  auto it = textures.find(assetId);
  if (it != textures.end()) {
    SDL_DestroyTexture(it->second);
    it->second = texture;
  } else {
    textures.emplace(assetId, texture);
  }

  logger.Log("New texture added to the Asset Store with id = " + assetId);
}

SDL_Texture *AssetStore::GetTexture(const std::string &assetId) const {
  // Callers null-check the result, so a missing id returns nullptr rather
  // than throwing out of std::map::at.
  auto it = textures.find(assetId);
  return (it != textures.end()) ? it->second : nullptr;
}
void AssetStore::AddFont(const std::string &assetId,
                         const std::string &filePath, int ptSize) {
  // TTF_OpenFont on a live font is the whole reason this cache exists: it
  // reads the file and builds a rasteriser every call, and a game that opened
  // one per DrawText paid that on every line of every frame.
  TTF_Font *font = TTF_OpenFont(filePath.c_str(), ptSize);
  if (!font) {
    logger.Err("AssetStore: failed to open font '" + filePath + "' at " +
               std::to_string(ptSize) + "pt - " + std::string(TTF_GetError()));
    return;
  }

  auto it = fonts.find(assetId);
  if (it != fonts.end()) {
    TTF_CloseFont(it->second);
    it->second = font;
  } else {
    fonts.emplace(assetId, font);
  }

  logger.Log("New font added to the Asset Store with id = " + assetId);
}

TTF_Font *AssetStore::GetFont(const std::string &assetId) const {
  auto it = fonts.find(assetId);
  return (it != fonts.end()) ? it->second : nullptr;
}

void AssetStore::AddSound(const std::string &assetId,
                          const std::string &filePath) {
  Mix_Chunk *chunk = Mix_LoadWAV(filePath.c_str());
  if (!chunk) {
    logger.Err("AssetStore: failed to load sound '" + filePath + "' - " +
               std::string(Mix_GetError()));
    return;
  }

  auto it = sounds.find(assetId);
  if (it != sounds.end()) {
    Mix_FreeChunk(it->second);
    it->second = chunk;
  } else {
    sounds.emplace(assetId, chunk);
  }

  logger.Log("New sound added to the Asset Store with id = " + assetId);
}

Mix_Chunk *AssetStore::GetSound(const std::string &assetId) const {
  auto it = sounds.find(assetId);
  return (it != sounds.end()) ? it->second : nullptr;
}

} // namespace storm
