#pragma once

// GENERATED FILE -- DO NOT EDIT.
//
// Written by scripts/generate-version.py from Makefile.debian's VERSION. Edit
// that instead and re-run the generator; the file is committed so a consumer
// of the installed engine needs no Python, and `--check` exists so a stale
// copy cannot survive that. It is the same arrangement as
// specs/compat/bridgedNames.h, and for the same reason.
//
// WHY A HEADER AT ALL. A compile flag is not a property of the artifact: the
// engine is a library and cannot print, so "which engine was this built
// against" has to be answerable by the binary itself. A game that writes its
// version into its own source, or reads it out of a makefile at runtime, is
// answering a different question than the one being asked -- and the day the
// two disagree, the disagreement is invisible.

namespace storm {

// The version this translation unit was compiled against, as a string.
inline constexpr const char *kEngineVersion = "2.7.0";

// The numeric triple, for a consumer that needs to compare rather than print.
// Derived from the version above at generation time, so there is still only
// one place the number is written.
inline constexpr int kEngineVersionMajor = 2;
inline constexpr int kEngineVersionMinor = 7;
inline constexpr int kEngineVersionPatch = 0;

// The same number with a leading "v" -- "v" followed by kEngineVersion. Kept as
// its own constant rather than built at runtime so a crash banner can print it
// from a signal handler, and so nobody has to remember to prefix the "v" at
// every call site, which is the half that gets forgotten.
inline constexpr const char *kEngineVersionString = "v2.7.0";

inline const char *VersionString() { return kEngineVersionString; }

// A build may be told a different version than the tree declared -- the
// release workflow overrides VERSION from the tag -- so a consumer that has
// the build's own idea can compare it here rather than assuming.
inline bool VersionEquals(const char *other) {
  if (other == nullptr) {
    return false;
  }
  for (int i = 0; kEngineVersion[i] != '\0' || other[i] != '\0'; ++i) {
    if (kEngineVersion[i] != other[i]) {
      return false;
    }
  }
  return true;
}

} // namespace storm
