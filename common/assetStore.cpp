#include "assetStore.h"

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
    logger.Err("AssetStore: failed to create texture for '" + assetId +
               "' — " + std::string(SDL_GetError()));
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