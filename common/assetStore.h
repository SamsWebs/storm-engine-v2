#pragma once

#include "SDL2/SDL_image.h"
#include <SDL2/SDL.h>
#include <map>
#include <string>

#include "logger.h"

class AssetStore {
private:
  using AssetContainer = std::map<std::string, SDL_Texture *>;
  AssetContainer textures;
  Logger logger;

public:
  AssetStore();
  ~AssetStore();

  void ClearAssets();
  void AddTexture(SDL_Renderer *renderer, const std::string &assetId,
                  const std::string &filePath);
  SDL_Texture *GetTexture(const std::string &assetId) const;
};
