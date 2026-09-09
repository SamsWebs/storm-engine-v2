#pragma once

#include <map>
#include <memory>
#include <string>

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_mixer.h>
#include <SDL2/SDL_ttf.h>

#include "logger.h"

#include <fstream>
#include <memory>

namespace storm {

struct PackReader; // packFile.h — the pack overloads take it by reference

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
  using FontBlobContainer = std::map<std::string, std::vector<uint8_t>>;
  using SoundContainer = std::map<std::string, Mix_Chunk *>;

  TextureContainer textures;
  FontContainer fonts;
  // SDL_ttf reads glyphs lazily out of the RWops it was opened with, so the
  // blob behind a pack-loaded font must outlive the font. Kept parallel to
  // `fonts` by id; emptied in ClearAssets.
  FontBlobContainer fontBlobs;
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

  // THE WIRING CHOKE POINT. Loads the game's asset pack once; afterwards
  // every path-based Add whose blob is IN the pack loads from it, with the
  // entry name being the path minus its leading "assets/" (the game writes
  // paths as "assets/gfx/x.png", the pack's entries are relative to the
  // assets root). Paths the pack does not hold fall back to the loose file,
  // so a development tree with a partial pack still works and no call site
  // changes. False for a missing or corrupt pack file: logged, the pack is
  // simply inactive, the loose contract stands. Loading again replaces the
  // pack. The store owns the stream for the store's lifetime; the pack is
  // NOT freed by ClearAssets (it holds no asset instances).
  bool LoadPack(const std::string &pakPath);

  // Pack-file variants: the blob comes from a storm::PackReader instead of
  // a file path. Same error contract - a missing entry, an empty blob, an
  // oversized one or undecodable bytes log and store nothing. The texture
  // and sound loaders decode synchronously before freeing the RWops; the
  // font loader does NOT - SDL_ttf retains the RWops and reads glyphs
  // lazily at render time, so the store keeps the font's blob alive in
  // fontBlobs for the font's lifetime.
  void AddTexture(SDL_Renderer *renderer, const std::string &assetId,
                  const PackReader &pack, const std::string &entryName);

  // A TTF_Font is rasterised at one point size, so a game that draws at two
  // sizes stores two ids ("hud-18", "title-32"). Re-adding an id replaces and
  // frees the old font.
  void AddFont(const std::string &assetId, const std::string &filePath,
               int ptSize);
  void AddFont(const std::string &assetId, const PackReader &pack,
               const std::string &entryName, int ptSize);
  TTF_Font *GetFont(const std::string &assetId) const;

  void AddSound(const std::string &assetId, const std::string &filePath);
  void AddSound(const std::string &assetId, const PackReader &pack,
                const std::string &entryName);
  Mix_Chunk *GetSound(const std::string &assetId) const;

private:
  // One decision, CODING.md tenet 1: re-adding an id replaces (and frees)
  // the old asset instead of silently leaking the new one. Every Add method
  // ends here; none keeps its own copy of the branch.
  template <typename Container, typename Asset, typename FreeFn>
  void ReplaceOrStore(Container &container, const std::string &assetId,
                      Asset *asset, FreeFn freeFn) {
    auto it = container.find(assetId);
    if (it != container.end()) {
      freeFn(it->second);
      it->second = asset;
    } else {
      container.emplace(assetId, asset);
    }
  }

  // Shared tails: hand a decoded asset to the store and log the outcome.
  // The path-based and pack-based Add methods converge on these. StoreFont
  // takes ownership of `blob` when the font was opened from memory; the
  // path-based overload passes an empty vector, which erases any stale blob
  // a previous pack font under the same id owned.
  void StoreTexture(SDL_Renderer *renderer, const std::string &assetId,
                    SDL_Surface *surface);
  void StoreFont(const std::string &assetId, TTF_Font *font,
                 std::vector<uint8_t> blob = {});
  void StoreSound(const std::string &assetId, Mix_Chunk *chunk);

  // Reads one pack entry for an Add overload and enforces the guards every
  // caller shares (missing entry, empty blob, > INT_MAX bytes - the int
  // SDL_RWFromMem takes would truncate larger sizes). Returns an RWops over
  // the bytes, or nullptr after logging; the caller keeps `bytes` alive as
  // long as it uses the RWops.
  SDL_RWops *OpenPackEntry(const PackReader &pack, const std::string &entryName,
                           const std::string &assetId, const char *kind,
                           std::vector<uint8_t> &bytes);

  // One decision for all three path-based Adds (CODING.md tenet 1): is this
  // blob in the loaded pack? The entry-name convention lives here — the
  // path minus its leading "assets/" — so a game that names its pack
  // entries differently can never half-match.
  bool InPack(const std::string &filePath) const;
  std::string PackEntryName(const std::string &filePath) const;

  // The pack stream and reader, owned for the store's lifetime. The font
  // lazy-read rule makes the STREAM outlive every pack-loaded font, which
  // is why these are members and not locals in LoadPack.
  std::unique_ptr<std::ifstream> packStream_;
  std::unique_ptr<PackReader> pack_;
};

typedef std::unique_ptr<AssetStore> AssetStore_Ptr;

} // namespace storm
