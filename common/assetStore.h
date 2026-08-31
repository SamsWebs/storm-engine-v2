#pragma once

#include <map>
#include <memory>
#include <string>

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_mixer.h>
#include <SDL2/SDL_ttf.h>

#include "logger.h"

namespace storm {

// Caches the three asset kinds the engine already links against. Every getter
// returns nullptr for a missing id rather than throwing, so callers null-check
// and nothing aborts under the Switch build's -fno-exceptions.
//
// **Call ClearAssets() before TTF_Quit(), Mix_CloseAudio() or SDL_Quit().**
// Those calls free every open font and chunk themselves, so a store destroyed
// afterwards hands already-freed pointers to TTF_CloseFont / Mix_FreeChunk.
// The store is usually owned by the Game and outlives the state that shut the
// subsystems down, which is exactly the order that goes wrong.
//
// The store does not initialise SDL_ttf or SDL_mixer. A game that never calls
// TTF_Init() or Mix_OpenAudio() gets a logged failure and a nullptr, not a
// crash.
class AssetStore {
private:
  using TextureContainer = std::map<std::string, SDL_Texture *>;
  using FontContainer = std::map<std::string, TTF_Font *>;
  using SoundContainer = std::map<std::string, Mix_Chunk *>;

  TextureContainer textures;
  FontContainer fonts;
  SoundContainer sounds;
  Logger logger;

public:
  AssetStore();
  ~AssetStore();

  // Frees every texture, font and sound, and empties the store.
  void ClearAssets();

  void AddTexture(SDL_Renderer *renderer, const std::string &assetId,
                  const std::string &filePath);
  SDL_Texture *GetTexture(const std::string &assetId) const;

  // A TTF_Font is rasterised at one point size, so a game that draws at two
  // sizes stores two ids ("hud-18", "title-32"). Re-adding an id replaces and
  // frees the old font.
  void AddFont(const std::string &assetId, const std::string &filePath,
               int ptSize);
  TTF_Font *GetFont(const std::string &assetId) const;

  void AddSound(const std::string &assetId, const std::string &filePath);
  Mix_Chunk *GetSound(const std::string &assetId) const;
};

typedef std::unique_ptr<AssetStore> AssetStore_Ptr;

} // namespace storm
