#include "assetStore.h"

#include <climits>

#include "packFile.h"

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
  fontBlobs.clear(); // the blobs pack-loaded fonts lazily read from

  for (auto &sound : sounds) {
    Mix_FreeChunk(sound.second);
  }
  sounds.clear();
}

void AssetStore::StoreTexture(SDL_Renderer *renderer,
                              const std::string &assetId,
                              SDL_Surface *surface) {
  SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
  SDL_FreeSurface(surface);
  if (!texture) {
    logger.Err("AssetStore: failed to create texture for '" + assetId + "' — " +
               std::string(SDL_GetError()));
    return;
  }

  ReplaceOrStore(textures, assetId, texture, SDL_DestroyTexture);
  logger.Log("New texture added to the Asset Store with id = " + assetId);
}

void AssetStore::AddTexture(SDL_Renderer *renderer, const std::string &assetId,
                            const std::string &filePath) {
  if (InPack(filePath)) {
    AddTexture(renderer, assetId, *pack_, PackEntryName(filePath));
    return;
  }
  SDL_Surface *surface = IMG_Load(filePath.c_str());
  if (!surface) {
    logger.Err("AssetStore: failed to load '" + filePath + "' — " +
               std::string(IMG_GetError()));
    return;
  }
  StoreTexture(renderer, assetId, surface);
}

bool AssetStore::LoadPack(const std::string &pakPath) {
  auto stream = std::make_unique<std::ifstream>(pakPath, std::ios::binary);
  auto reader = std::make_unique<PackReader>();
  if (!*stream || !reader->Open(*stream)) {
    logger.Err("AssetStore: no usable pack at '" + pakPath +
               "' - the loose-file contract stands");
    packStream_.reset();
    pack_.reset();
    return false;
  }
  packStream_ = std::move(stream);
  pack_ = std::move(reader);
  logger.Log("AssetStore: asset pack loaded from '" + pakPath + "' (" +
             std::to_string(pack_->Count()) + " entries)");
  return true;
}

bool AssetStore::InPack(const std::string &filePath) const {
  if (pack_ == nullptr) {
    return false;
  }
  return pack_->Has(PackEntryName(filePath));
}

std::string AssetStore::PackEntryName(const std::string &filePath) const {
  // The convention, in one place: the path as the game writes it minus its
  // leading "assets/" (the pack's entries are relative to the assets root).
  // The optional "./" comes first - the consuming game writes
  // "./assets/gfx/x.png", and CWD-relative paths commonly carry it.
  std::string path = filePath;
  static const std::string kDot = "./";
  if (path.rfind(kDot, 0) == 0) {
    path.erase(0, kDot.size());
  }
  static const std::string kPrefix = "assets/";
  if (path.rfind(kPrefix, 0) == 0) {
    return path.substr(kPrefix.size());
  }
  return path;
}

SDL_RWops *AssetStore::OpenPackEntry(const PackReader &pack,
                                     const std::string &entryName,
                                     const std::string &assetId,
                                     const char *kind,
                                     std::vector<uint8_t> &bytes) {
  if (!pack.Read(entryName, bytes)) {
    logger.Err("AssetStore: entry '" + entryName + "' not found in pack for " +
               kind + " '" + assetId + "'");
    return nullptr;
  }
  if (bytes.empty()) {
    logger.Err("AssetStore: entry '" + entryName + "' in pack is empty");
    return nullptr;
  }
  if (bytes.size() > static_cast<std::size_t>(INT_MAX)) {
    logger.Err("AssetStore: entry '" + entryName + "' in pack is too large");
    return nullptr;
  }
  SDL_RWops *src = SDL_RWFromMem(bytes.data(), static_cast<int>(bytes.size()));
  if (!src) {
    logger.Err("AssetStore: failed to open pack entry '" + entryName + "' — " +
               std::string(SDL_GetError()));
  }
  return src;
}

void AssetStore::AddTexture(SDL_Renderer *renderer, const std::string &assetId,
                            const PackReader &pack,
                            const std::string &entryName) {
  std::vector<uint8_t> bytes;
  SDL_RWops *src = OpenPackEntry(pack, entryName, assetId, "texture", bytes);
  if (!src) {
    return;
  }

  // SDL_image decodes synchronously and frees the RWops on every path, so
  // the blob's lifetime can end with this call.
  SDL_Surface *surface = IMG_Load_RW(src, 1);
  if (!surface) {
    logger.Err("AssetStore: failed to load texture '" + assetId +
               "' from pack entry '" + entryName + "' — " +
               std::string(IMG_GetError()));
    return;
  }
  StoreTexture(renderer, assetId, surface);
}

SDL_Texture *AssetStore::GetTexture(const std::string &assetId) const {
  // Callers null-check the result, so a missing id returns nullptr rather
  // than throwing out of std::map::at.
  auto it = textures.find(assetId);
  return (it != textures.end()) ? it->second : nullptr;
}

void AssetStore::StoreFont(const std::string &assetId, TTF_Font *font,
                           std::vector<uint8_t> blob) {
  ReplaceOrStore(fonts, assetId, font, TTF_CloseFont);
  // The blob, when there is one, is what the font lazily reads glyphs from;
  // it must outlive the font, which it does here in fontBlobs. An empty
  // blob (path-based load) erases any stale blob an older pack font under
  // the same id owned.
  if (blob.empty()) {
    fontBlobs.erase(assetId);
  } else {
    fontBlobs[assetId] = std::move(blob);
  }
  logger.Log("New font added to the Asset Store with id = " + assetId);
}

void AssetStore::AddFont(const std::string &assetId,
                         const std::string &filePath, int ptSize) {
  if (InPack(filePath)) {
    AddFont(assetId, *pack_, PackEntryName(filePath), ptSize);
    return;
  }
  // TTF_OpenFont on a live font is the whole reason this cache exists: it
  // reads the file and builds a rasteriser every call, and a game that opened
  // one per DrawText paid that on every line of every frame.
  TTF_Font *font = TTF_OpenFont(filePath.c_str(), ptSize);
  if (!font) {
    logger.Err("AssetStore: failed to open font '" + filePath + "' at " +
               std::to_string(ptSize) + "pt - " + std::string(TTF_GetError()));
    return;
  }
  StoreFont(assetId, font);
}

void AssetStore::AddFont(const std::string &assetId, const PackReader &pack,
                         const std::string &entryName, int ptSize) {
  std::vector<uint8_t> bytes;
  SDL_RWops *src = OpenPackEntry(pack, entryName, assetId, "font", bytes);
  if (!src) {
    return;
  }

  // SDL_ttf does NOT read the font up front: it keeps the RWops open and
  // reads glyphs lazily at render time, closing it only in TTF_CloseFont
  // (font->freesrc = 1). So the blob must outlive the font, and the store
  // takes ownership of it via StoreFont. On failure SDL_ttf frees the
  // RWops immediately.
  TTF_Font *font = TTF_OpenFontRW(src, 1, ptSize);
  if (!font) {
    logger.Err("AssetStore: failed to open font '" + assetId + "' at " +
               std::to_string(ptSize) + "pt from pack entry '" + entryName +
               "' - " + std::string(TTF_GetError()));
    return;
  }
  StoreFont(assetId, font, std::move(bytes));
}

TTF_Font *AssetStore::GetFont(const std::string &assetId) const {
  auto it = fonts.find(assetId);
  return (it != fonts.end()) ? it->second : nullptr;
}

void AssetStore::StoreSound(const std::string &assetId, Mix_Chunk *chunk) {
  ReplaceOrStore(sounds, assetId, chunk, Mix_FreeChunk);
  logger.Log("New sound added to the Asset Store with id = " + assetId);
}

void AssetStore::AddSound(const std::string &assetId,
                          const std::string &filePath) {
  if (InPack(filePath)) {
    AddSound(assetId, *pack_, PackEntryName(filePath));
    return;
  }
  Mix_Chunk *chunk = Mix_LoadWAV(filePath.c_str());
  if (!chunk) {
    logger.Err("AssetStore: failed to load sound '" + filePath + "' - " +
               std::string(Mix_GetError()));
    return;
  }
  StoreSound(assetId, chunk);
}

void AssetStore::AddSound(const std::string &assetId, const PackReader &pack,
                          const std::string &entryName) {
  std::vector<uint8_t> bytes;
  SDL_RWops *src = OpenPackEntry(pack, entryName, assetId, "sound", bytes);
  if (!src) {
    return;
  }

  // SDL_mixer's loaders decode synchronously and free the RWops on every
  // path, so the blob's lifetime can end with this call.
  Mix_Chunk *chunk = Mix_LoadWAV_RW(src, 1);
  if (!chunk) {
    logger.Err("AssetStore: failed to load sound '" + assetId +
               "' from pack entry '" + entryName + "' - " +
               std::string(Mix_GetError()));
    return;
  }
  StoreSound(assetId, chunk);
}

Mix_Chunk *AssetStore::GetSound(const std::string &assetId) const {
  auto it = sounds.find(assetId);
  return (it != sounds.end()) ? it->second : nullptr;
}

} // namespace storm
