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
  SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
  SDL_FreeSurface(surface);

  // Add the texture to the map
  textures.emplace(assetId, texture);

  logger.Log("New texture added to the Asset Store with id = " + assetId);
}

SDL_Texture *AssetStore::GetTexture(const std::string &assetId) const {
  return textures.at(assetId);
}