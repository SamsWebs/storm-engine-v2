#pragma once

#include <map>
#include <memory>
#include <string>

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

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

typedef std::unique_ptr<AssetStore> AssetStore_Ptr;
