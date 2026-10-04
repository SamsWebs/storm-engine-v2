// The asset path seam — 2.5.2.
//
// The store had ONE path function, `AssetStore::PackEntryName`, and it was
// PRIVATE. That is the whole defect: a game cannot reuse a convention it
// cannot see, so a game reimplemented it, got it subtly wrong, and the result
// was that the pack was silently skipped for that asset — the failure mode
// `docs/assets.md` now describes instead of a downstream header comment
// guessing at it.
//
// The roadmap filed this as "AssetPath and AssetFilePath are already two
// functions". They are not. There is ONE concept wearing two hats: a path
// relative to the assets root, which is simultaneously the game's logical
// asset path and the pack's entry name, because the pack's entries ARE
// relative to the assets root. Saying so is the fix; two functions for one
// concept is what produced the `./` defect in the first place.
//
// Header-only and pure except for the two existence checks, which is
// deliberate: the path rules are the part worth spec'ing, and they are the
// part a game has to get right to interoperate with the pack at all.
#include <string>

#include <igloo/igloo_alt.h>

#include "../common/assetPath.h"

using namespace igloo;
using namespace storm;

Describe(AssetPathNormalising){
    It(strips_the_assets_prefix_because_the_pack_is_relative_to_that_root){
        // The convention, in one assertion. A pack built with entries relative
        // to its assets root has "gfx/x.png" in it, and the game writes
        // "assets/gfx/x.png", so the prefix is exactly what must come off.
        Assert::That(AssetPath("assets/gfx/x.png"), Equals("gfx/x.png"));
}

It(strips_a_leading_dot_slash_before_looking_for_the_assets_prefix) {
  // The defect that actually shipped. A game wrote "./assets/gfx/x.png",
  // the strip looked for "assets/" at position 0, did not find it, and
  // the pack was skipped for that asset with no diagnostic -- which reads
  // as "the pack is broken", not "one path was spelled differently".
  Assert::That(AssetPath("./assets/gfx/x.png"), Equals("gfx/x.png"));
}

It(leaves_an_already_canonical_path_alone) {
  Assert::That(AssetPath("gfx/x.png"), Equals("gfx/x.png"));
}

It(strips_at_most_one_assets_prefix) {
  // Repeat stripping would be a bug dressed as thoroughness: a real
  // directory can be named "assets", and "assets/assets/x.png" is a
  // legitimate path meaning assets/assets/x.png.
  Assert::That(AssetPath("assets/assets/x.png"), Equals("assets/x.png"));
}

It(strips_a_dot_slash_only_at_the_front) {
  // A "./" in the middle is part of a name, not a decoration. Stripping
  // it would turn "a/./b" into "a/b" and quietly point at a different
  // asset than the one the game named.
  Assert::That(AssetPath("gfx/./x.png"), Equals("gfx/./x.png"));
}

It(normalises_windows_separators_because_a_pack_entry_always_uses_slashes) {
  // A pack built on Windows and shipped to Linux is the case that
  // matters, and the entry names inside it are forward-slashed whatever
  // built them. Without this, the same game loads from the pack on the
  // author's machine and falls back to loose files on every other one.
  Assert::That(AssetPath("assets\\gfx\\x.png"), Equals("gfx/x.png"));
}

It(is_idempotent) {
  // The property that makes it safe to normalise twice, and the one that
  // would break first if a second strip were ever added.
  const char *paths[] = {"assets/gfx/x.png",
                         "./assets/gfx/x.png",
                         "gfx/x.png",
                         "assets\\gfx\\x.png",
                         "",
                         "./"};
  for (const char *path : paths) {
    const std::string once = AssetPath(path);
    Assert::That(AssetPath(once), Equals(once));
  }
}

It(returns_an_empty_string_for_an_empty_path_rather_than_a_slash) {
  // An empty asset path must stay empty. A base-joining function that
  // turns "" into "/" produces a path that looks valid and names the
  // filesystem root, which is not a diagnostic anyone wants to debug.
  Assert::That(AssetPath(""), Equals(""));
}

It(does_not_resolve_a_parent_reference) {
  // Deliberately not a filesystem normaliser. "assets/../secret" is
  // normalised to "secret" by lexical rules and to something else
  // entirely by the filesystem, and a pack entry name is a KEY, not a
  // location -- collapsing the two would let a name address outside the
  // pack.
  Assert::That(AssetPath("assets/../secret"), Equals("../secret"));
}
}
;

Describe(AssetPathJoining){It(puts_the_leaf_under_the_base){
    Assert::That(AssetFilePath("gfx/x.png", "assets"),
                 Equals("assets/gfx/x.png"));
}

It(leaves_the_path_alone_when_there_is_no_base) {
  // A base-less caller still needs a usable string, and "" is the
  // documented "use the path as written" case -- not a request for a
  // leading separator.
  Assert::That(AssetFilePath("gfx/x.png", ""), Equals("gfx/x.png"));
}

It(does_not_double_the_separator_when_the_base_already_ends_in_one) {
  Assert::That(AssetFilePath("gfx/x.png", "assets/"),
               Equals("assets/gfx/x.png"));
}

It(produces_the_same_path_for_the_override_base_as_for_the_readable_base) {
  // Asserted rather than assumed: the two bases are the SAME shape of
  // thing, and the whole point of the seam is that a replacement file
  // sits at the same relative position as the file it replaces. If the
  // override base ever produced a different shape, a player's
  // replacement would land where the loader is not looking.
  Assert::That(AssetOverridePath("gfx/x.png", "userdata/mods"),
               Equals("userdata/mods/gfx/x.png"));
  Assert::That(AssetOverridePath("gfx/x.png", "assets"),
               Equals(AssetFilePath("gfx/x.png", "assets")));
}

It(takes_the_path_a_game_wrote_rather_than_only_an_already_normal_one) {
  // The caller's most natural call is ResolveAssetFile("assets/gfx/x.png",
  // ...) and the normalising must happen inside, or every caller has to
  // remember to do it and one of them will not.
  Assert::That(AssetFilePath("./assets/gfx/x.png", "assets"),
               Equals("assets/gfx/x.png"));
}
}
;

Describe(AssetOverrideResolution) {
  It(prefers_a_replacement_over_the_shipped_file) {
    // "Shipped content is read-only, the player's replacement lives beside
    // the save" as one call, rather than a per-game convention that has to
    // be re-implemented and got wrong.
    const std::string base = "./specs/assets";
    Assert::That(ResolveAssetFile("images/white8.bmp", base,
                                  "./specs/assets/overrides/existing"),
                 Equals("./specs/assets/overrides/existing/images/white8.bmp"));
  };

  It(falls_back_to_the_shipped_file_when_there_is_no_replacement) {
    const std::string base = "./specs/assets";
    Assert::That(ResolveAssetFile("images/white8.bmp", base,
                                  "./specs/assets/overrides/missing"),
                 Equals("./specs/assets/images/white8.bmp"));
  };

  It(falls_back_when_there_is_no_override_base_at_all) {
    // A game with no mod support passes "" and must get the shipped file,
    // not an empty string it has to special-case.
    Assert::That(ResolveAssetFile("images/white8.bmp", "./specs/assets", ""),
                 Equals("./specs/assets/images/white8.bmp"));
  }

  It(does_not_treat_a_file_at_the_working_directory_as_an_override) {
    // The empty-writable-base case, made observable. With a naive existence
    // probe, an empty base resolves the override candidate to the BARE asset
    // path, and any file of that name sitting in the working directory is then
    // picked up as a "replacement". The suite runs from the repo root, so
    // "Makefile.debian" is a real file there and
    // "./specs/assets/Makefile.debian" is not. Without this case a sabotage
    // that drops the empty-base guard passes every other assertion here,
    // because on a machine where the bare path happens not to exist, the broken
    // code and the correct code agree.
    std::string which;
    const std::string resolved =
        ResolveAssetFile("Makefile.debian", "./specs/assets", "", &which);
    Assert::That(resolved, Equals("./specs/assets/Makefile.debian"));
    Assert::That(which, Equals("shipped"));
  };

  It(does_not_accept_a_directory_as_a_replacement) {
    // Measured, not assumed: `std::ifstream(dir).good()` is TRUE on Linux,
    // because the failure surfaces on the first read rather than the open. A
    // directory sitting where an override should be would therefore be accepted
    // and handed to a loader, which fails in a way that says nothing about a
    // stray folder.
    Assert::That(AssetOverrideExists("./specs/assets/overrides"),
                 Equals(false));
    Assert::That(AssetOverrideExists(
                     "./specs/assets/overrides/existing/images/white8.bmp"),
                 Equals(true));
  };

  It(never_treats_an_empty_path_as_a_replacement) {
    // libstdc++ already returns false here, so this documents the contract
    // rather than catching a defect present today -- said so rather than left
    // to look like a bug catch. It is the case that would matter on a standard
    // library where opening "" resolves to the working directory.
    Assert::That(AssetOverrideExists(""), Equals(false));
  };

  It(tells_the_caller_which_one_it_chose) {
    // A loader that silently prefers a replacement is a support ticket
    // nobody can answer: the game shows the wrong art and the developer
    // cannot tell whether the pack, the override or the file is at fault.
    std::string which;
    const std::string base = "./specs/assets";
    ResolveAssetFile("images/white8.bmp", base,
                     "./specs/assets/overrides/existing", &which);
    Assert::That(which, Equals("override"));
  }

  It(says_shipped_when_it_falls_back) {
    std::string which;
    ResolveAssetFile("images/white8.bmp", "./specs/assets",
                     "./specs/assets/overrides/missing", &which);
    Assert::That(which, Equals("shipped"));
  }
};
