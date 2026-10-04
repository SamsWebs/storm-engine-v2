#pragma once

// The `.map` format version header — 2.5.1.
//
// A `.map` had no version, so a reader could not tell a field that was ADDED
// from one that still parses and now means something else. The config contract
// this repo already follows for its own files — *unknown keys are ignored* —
// covers the first case and not the second, and 2.0.0's `Tile` change is the
// proof the format moves: the animation fields were appended to both the struct
// and the record, and every map written before that is still a map on
// somebody's disk.
//
// ONE header, shared by both writers and both readers. The engine's
// `TileMapLoader` and `editor/src/utilities/FileLoader.cpp` used to be two
// independent implementations of the same format, in two binaries, which meant
// a change to the format had to be made twice and the second time was the one
// that got forgotten — and the editor is a separate artifact that has to be
// rebuilt and repackaged in the same release as the engine. Both now call
// `TileMapVersionLine()` and both refuse what they do not understand, so there
// is no second parser to drift.
//
// Header-only and pure: the reader takes a `std::istream`, so a spec needs no
// file and no filesystem. No `.cpp` in `common/` for this, deliberately — a new
// one has to be added to four build files or it silently fails to reach Switch
// and Android, and there is nothing here that needs a translation unit.
#include <cstdlib>
#include <istream>
#include <string>

namespace storm {

// The token that opens a versioned `.map`. Alpha-leading on purpose: the
// loader's format sniffing reads the first non-space character and calls a map
// "editor format" when it is a letter, so a magic starting with a digit would
// be sniffed as CSV and take the other branch entirely.
inline constexpr const char *kTileMapFormatMagic = "storm-map";

// The current record layout.
//
// This is 1, and that is not a dodge: the header line is itself version 1 of
// the versioned format, and the record layout behind it is unchanged from what
// every existing map already contains. Inventing a 2 would be a number with no
// meaning behind it, and the next person to bump it would have no way to know
// which maps it was protecting. Bump it when the RECORD changes, and the
// matching `.map` files in `specs/assets/tilemaps/` are where the new shape is
// pinned.
inline constexpr int kTileMapFormatVersion = 1;

// The oldest layout this build still reads. Currently the same as the current
// one; it is a separate constant because "refuse anything older" and "refuse
// anything newer" are different decisions and 2.4.1's `gameStateBase.h` split
// is the precedent for keeping them apart.
inline constexpr int kTileMapFormatMinVersion = 1;

// What the header said, and what to do about it.
enum class TileMapVersionState {
  Unversioned, // no header: a map written before the header existed
  Supported,   // header present and in range
  TooNew,      // written by a newer engine; half-reading it would produce a
               // level that renders and is wrong, so it is refused instead
  TooOld,      // below the floor this build still reads
  Malformed,   // the magic is there and the version is not a usable number
};

struct TileMapVersion {
  TileMapVersionState state = TileMapVersionState::Unversioned;
  // The version to read the file WITH. An unversioned file is read as the
  // oldest format, which is what keeps every pre-header map loading.
  int version = kTileMapFormatMinVersion;
  // What the file actually declared, or 0 when it declared nothing usable
  // -- including a header whose version was not a number. `state` is what
  // says whether that is a problem. Kept
  // separate from `version` on purpose: "the file said nothing" and "the file
  // said 1" are different facts, and collapsing them is the ambiguity the
  // header was added to remove.
  int declared = 0;
};

// Reads the version header from the START of `in`, consuming it. Leaves the
// stream positioned on the first tile record.
//
// The header is TWO TOKENS, not a line. Reading it as a line is the obvious
// implementation and the wrong one: a hand-edit that joined the header to the
// first record — or a writer that forgot the newline — would then drop that
// record silently, and a level missing its first tile is not a visible bug.
// Token reading survives both, and since the format is whitespace-separated
// throughout, it costs nothing.
inline TileMapVersion ReadTileMapVersion(std::istream &in) {
  TileMapVersion result;

  // Remember where we started, because on the unversioned path the probe
  // below reads the first record's group name and has to PUT IT BACK. The
  // obvious implementation -- read a token, and if it is not the magic,
  // return as though nothing was read -- silently eats that token, and every
  // caller then parses a map one field to the left: the group name becomes the
  // asset id, the asset id becomes the tile width, and nothing complains,
  // because every field is still an int or a string. Rewinding is the only
  // version of this that is correct, and it costs one tellg.
  //
  // std::istream is the parameter rather than std::ifstream so a spec can pass
  // a stringstream; a seekg that fails leaves the stream wherever the probe
  // stopped, which is the old behaviour, so a non-seekable stream degrades to
  // "wrong" rather than to "throwing". Nothing here is a pipe.
  const std::streampos start = in.tellg();

  std::string token;
  if (!(in >> token)) {
    // Empty file. A map with no tiles, not an error -- the editor writes
    // one when a canvas is saved empty, and refusing it would break
    // save-then-load.
    return result;
  }
  if (token != kTileMapFormatMagic) {
    // Not a header: it is the first record's group name, and the caller
    // needs to read it.
    if (start != std::streampos(-1)) {
      in.clear();
      in.seekg(start);
    }
    return result;
  }
  std::string number;
  if (!(in >> number)) {
    result.state = TileMapVersionState::Malformed;
    return result;
  }

  // strtol, not std::stoi: the latter throws std::invalid_argument on
  // "next", and this header is parsed from a file on the load path, where a
  // malformed map must not take the process down. Switch also builds with
  // -fno-exceptions, so a throw here is an abort there.
  const std::string trimmed = [&number] {
    std::size_t end = number.find_first_not_of("+-");
    if (end == std::string::npos) {
      return std::string();
    }
    return number.substr(end);
  }();
  if (trimmed.empty() ||
      trimmed.find_first_not_of("0123456789") != std::string::npos) {
    result.state = TileMapVersionState::Malformed;
    return result;
  }
  const long value = std::strtol(number.c_str(), nullptr, 10);
  result.declared = static_cast<int>(value);
  if (value > kTileMapFormatVersion) {
    result.state = TileMapVersionState::TooNew;
    return result;
  }
  if (value < kTileMapFormatMinVersion) {
    result.state = TileMapVersionState::TooOld;
    return result;
  }
  result.state = TileMapVersionState::Supported;
  result.version = static_cast<int>(value);
  return result;
}

// The line a writer puts at the top of a `.map`. Includes its own newline, so
// a writer cannot forget one and silently join the header to the first record.
//
// Deliberately produced by the SAME constant the reader enforces, so the
// round trip cannot drift: a writer stamping one number and a reader enforcing
// another would still pass any test that only round-trips, which is exactly the
// failure the shared constant is here to prevent.
inline std::string TileMapVersionLine() {
  return std::string(kTileMapFormatMagic) + " " +
         std::to_string(kTileMapFormatVersion) + "\n";
}

// The refusal, in one place, so the loader and the editor cannot word it
// differently. Empty for a version that is fine to read -- there is nothing to
// refuse and nothing to say.
inline std::string TileMapVersionRefusal(const TileMapVersion &version,
                                         const std::string &fileMap) {
  switch (version.state) {
  case TileMapVersionState::Unversioned:
  case TileMapVersionState::Supported:
    return std::string();
  case TileMapVersionState::TooNew:
    return "TileMapLoader: '" + fileMap +
           "': written by a newer engine "
           "(format version " +
           std::to_string(version.declared) + "; this build reads " +
           std::to_string(kTileMapFormatMinVersion) + " to " +
           std::to_string(kTileMapFormatVersion) +
           "). Refusing rather than half-reading it, because a map that loads "
           "and is wrong is worse than one that does not load. Upgrade the "
           "engine, or re-save the map from the editor.";
  case TileMapVersionState::TooOld:
    return "TileMapLoader: '" + fileMap + "': format version " +
           std::to_string(version.declared) +
           " is older than this build reads (floor is " +
           std::to_string(kTileMapFormatMinVersion) + "). Refusing.";
  case TileMapVersionState::Malformed:
    return "TileMapLoader: '" + fileMap + "': starts with the '" +
           kTileMapFormatMagic +
           "' format header but does not declare a version number. Refusing, "
           "rather than falling through to the old parser.";
  }
  return std::string();
}

} // namespace storm
