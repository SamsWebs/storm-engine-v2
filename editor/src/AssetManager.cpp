#include "AssetManager.h"

AssetManager::AssetManager() {}

AssetManager::~AssetManager() {}

void AssetManager::AddTexture(Renderer &renderer, const std::string &assetId,
                              const std::string &filePath) {
  if (!HasTexture(assetId)) {
    SDL_Surface *surface = IMG_Load(filePath.c_str());
    if (!surface) {
      logger.Err("AssetManager: unable to load [" + filePath + "] — " +
                 std::string(IMG_GetError()));
      return; // don't store a null texture
    }

    Texture texture =
        Texture(SDL_CreateTextureFromSurface(renderer.get(), surface));

    // Free the surface once the texture is created
    SDL_FreeSurface(surface);

    if (!texture) {
      logger.Err("AssetManager: unable to create texture [" + assetId +
                 "] from " + filePath);
      return; // don't store a null texture
    }

    // Add the Textures to the map
    mTextures.emplace(assetId, std::move(texture));
  } else {
    logger.Err("AssetManager: texture [" + assetId + "] already exists!");
  }
}

const Texture &AssetManager::GetTexture(const std::string &assetId) {
  // find(), not operator[] — a missing id must not insert a null texture
  // into the map and hand it out as if it loaded.
  static const Texture kNullTexture;
  auto it = mTextures.find(assetId);
  if (it == mTextures.end()) {
    logger.Err("AssetManager: no texture with id [" + assetId + "]");
    return kNullTexture;
  }
  return it->second;
}

bool AssetManager::HasTexture(const std::string &assetId) {
  return mTextures.find(assetId) != mTextures.end();
}

void AssetManager::RemoveTexture(const std::string &assetId) {}
