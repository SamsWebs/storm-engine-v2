#pragma once

// The asset path seam — 2.5.2.
//
// Three shapes, one rule, and a doc that says so: `docs/assets.md`.
//
// WHY THIS EXISTS
//
// `AssetStore` had ONE path function, `PackEntryName`, and it was **private**.
// That is the entire defect. A game cannot reuse a convention it cannot see, so
// a game reimplemented it, got it subtly wrong, and the result was that the
// asset silently fell back to the loose file — which looks like "the pack is
// broken", and sends the next person to debug the pack instead of the path.
// That is the `./` strip: a game wrote "./assets/gfx/x.png", the strip looked
// for "assets/" at position 0, did not find it, and the pack was skipped.
//
// ONE CONCEPT, NOT TWO
//
// This is worth being explicit about, because getting it wrong is what made
// the bug. There is not an "AssetPath" and an "AssetFilePath" that happen to
// agree: there is ONE path, relative to the assets root, and it is
// simultaneously the game's logical asset path and the pack's entry name,
// because a pack's entries ARE relative to the assets root. `AssetPath()`
// below is that one path. Everything else is the same path anchored at a
// different base.
//
// So:
//
//   "assets/gfx/player.png"      what a game writes
//            |  AssetPath
//            v
//   "gfx/player.png"             the pack entry name == the logical asset path
//            |  AssetFilePath(.., "assets")     -> "assets/gfx/player.png"
//            |  AssetOverridePath(.., "userdata")-> "userdata/gfx/player.png"
//
// Header-only. The path rules are pure and spec'd; the two existence checks
// are the only part that touches a filesystem, and they are small enough to say
// out loud.
#include <filesystem>
#include <string>

namespace storm {

// The canonical asset path: relative to the assets root, no leading "./", no
// leading "assets/", forward slashes only. This string is the pack's entry
// name as well as the game's logical path, which is why it is one function.
inline std::string AssetPath(const std::string &gamePath) {
  std::string path = gamePath;
  // Backslashes first and unconditionally: a pack is built on whatever
  // machine its author used, and the entry names inside it are forward-slashed
  // whatever built them. Without this a game loads from the pack on the
  // author's Windows machine and falls back to loose files everywhere else,
  // which is the same class of bug as the missing "./" strip and just as
  // invisible.
  for (char &c : path) {
    if (c == '\\') {
      c = '/';
    }
  }
  // "./" before "assets/", because a path carrying both spells it in that
  // order and stripping them in the other order finds neither.
  static const std::string kDotSlash = "./";
  while (path.rfind(kDotSlash, 0) == 0) {
    path.erase(0, kDotSlash.size());
  }
  // At most ONE "assets/". Repeat stripping would be thoroughness as a bug:
  // a real directory can be called "assets", and "assets/assets/x.png" means
  // assets/assets/x.png.
  static const std::string kAssetsPrefix = "assets/";
  if (path.rfind(kAssetsPrefix, 0) == 0) {
    path.erase(0, kAssetsPrefix.size());
  }
  return path;
}

// Joins a base and a leaf with exactly one separator, and treats an empty base
// as "no base" rather than "the filesystem root". Exposed because both of the
// two functions below are it, and a game writing its own override loader needs
// the same rule or it lands its files where the resolver is not looking.
inline std::string JoinAssetPath(const std::string &base,
                                 const std::string &leaf) {
  if (base.empty()) {
    return leaf;
  }
  if (leaf.empty()) {
    return base;
  }
  std::string joined = base;
  if (joined.back() == '/' || joined.back() == '\\') {
    joined += leaf;
  } else {
    joined += "/";
    joined += leaf;
  }
  return joined;
}

// Where the shipped file is opened at, for a game path the caller writes the
// way it always has ("assets/gfx/player.png"). Normalises internally, because
// requiring every caller to have normalised first is how one of them forgets.
inline std::string AssetFilePath(const std::string &gamePath,
                                 const std::string &readableBase) {
  return JoinAssetPath(readableBase, AssetPath(gamePath));
}

// Where a player's REPLACEMENT of that file would live: the same relative
// position under a writable base. Same shape as AssetFilePath by
// construction, which is the point — a replacement that lands somewhere else
// is a replacement the loader never finds.
inline std::string AssetOverridePath(const std::string &gamePath,
                                     const std::string &writableBase) {
  return JoinAssetPath(writableBase, AssetPath(gamePath));
}

// True when a real FILE is there.
//
// Two things a naive probe gets wrong, both measured on this platform rather
// than assumed:
//
//   - `std::ifstream("")` is already false, so the empty-path case needs no
//     special handling to be correct here. The explicit check stays anyway,
//     because the contract is that an empty path is never an override and
//     because another standard library may not agree -- an empty path must not
//     quietly become "the current directory" somewhere else.
//   - `std::ifstream(dir)` is **true**. Verified: opening a directory succeeds
//     on Linux, because the failure shows up on the first read, not the open.
//     So a directory that happens to sit where an override should be would be
//     accepted, handed to a loader, and produce a confusing decode failure
//     instead of a clear "no override here". Hence is_regular_file.
inline bool AssetOverrideExists(const std::string &overridePath) {
  if (overridePath.empty()) {
    return false;
  }
  std::error_code ec;
  return std::filesystem::is_regular_file(overridePath, ec) && !ec;
}

// "Shipped content is read-only, the player's replacement lives beside the
// save" as one call instead of a per-game convention.
//
// Returns the file to open, preferring a replacement when one exists, and says
// which it chose in `outKind` ("override" or "shipped") when asked. The
// `outKind` out-parameter is the part worth keeping: a loader that silently
// prefers a replacement is a support ticket nobody can answer, because the
// game shows the wrong art and nothing records that it looked.
inline std::string ResolveAssetFile(const std::string &gamePath,
                                    const std::string &readableBase,
                                    const std::string &writableBase,
                                    std::string *outKind = nullptr) {
  if (outKind != nullptr) {
    outKind->clear();
  }
  if (!writableBase.empty()) {
    const std::string candidate = AssetOverridePath(gamePath, writableBase);
    if (AssetOverrideExists(candidate)) {
      if (outKind != nullptr) {
        *outKind = "override";
      }
      return candidate;
    }
  }
  if (outKind != nullptr) {
    *outKind = "shipped";
  }
  return AssetFilePath(gamePath, readableBase);
}

} // namespace storm
