#include <igloo/igloo_alt.h>

#include <cstdio>
#include <fstream>
#include <sstream>
#include <vector>

#include "../common/assetStore.h"
#include "../common/packFile.h"
#include "../common/text.h"
#include "support/softwareRenderer.h"

using namespace igloo;
using namespace storm;

// Headless specs for the PackReader overloads of AssetStore. Same error
// contract as the path-based loads, pinned here: a missing pack entry, an
// empty blob or undecodable bytes log and store nothing — never a throw,
// never a silently stored null texture. The one real load uses the repo's
// 8x8 white BMP through the software renderer, which needs no window and no
// SDL_Init; fonts and sounds cannot be really loaded headless (TTF needs a
// real font asset, Mix needs an opened audio device), so their specs pin the
// failure paths only.
Describe(AssetStorePackSpec) {

  It(should_not_store_a_texture_when_the_pack_entry_is_missing) {
    std::ostringstream out;
    Assert::That(PackWriter().Save(out), Equals(true)); // valid, zero entries
    std::istringstream in(out.str());
    PackReader pack;
    Assert::That(pack.Open(in), Equals(true));

    // Declared after the target so the store dies first: its destructor
    // destroys textures, and that needs a live renderer.
    SpecSurfaceTarget target(8, 8);
    AssetStore store;
    store.AddTexture(target.renderer, "tex", pack, "no/such.png");
    Assert::That(store.GetTexture("tex") == nullptr, Equals(true));
  };

  It(should_not_store_a_texture_when_the_blob_is_undecodable) {
    PackWriter writer;
    Assert::That(writer.Add("junk.bin", std::vector<uint8_t>{1, 2, 3, 4}),
                 Equals(true));
    std::ostringstream out;
    Assert::That(writer.Save(out), Equals(true));
    std::istringstream in(out.str());
    PackReader pack;
    Assert::That(pack.Open(in), Equals(true));

    // Declared after the target so the store dies first: its destructor
    // destroys textures, and that needs a live renderer.
    SpecSurfaceTarget target(8, 8);
    AssetStore store;
    store.AddTexture(target.renderer, "tex", pack, "junk.bin");
    Assert::That(store.GetTexture("tex") == nullptr, Equals(true));
  };

  It(should_not_store_a_texture_when_the_blob_is_empty) {
    PackWriter writer;
    Assert::That(writer.Add("empty.bin", std::vector<uint8_t>{}), Equals(true));
    std::ostringstream out;
    Assert::That(writer.Save(out), Equals(true));
    std::istringstream in(out.str());
    PackReader pack;
    Assert::That(pack.Open(in), Equals(true));

    // Declared after the target so the store dies first: its destructor
    // destroys textures, and that needs a live renderer.
    SpecSurfaceTarget target(8, 8);
    AssetStore store;
    store.AddTexture(target.renderer, "tex", pack, "empty.bin");
    Assert::That(store.GetTexture("tex") == nullptr, Equals(true));
  };

  It(should_load_a_texture_from_a_pack_and_replace_on_re_add) {
    // The real load, and the only spec that exercises the store's
    // replace-on-re-add decision (CODING.md tenet 1): the second Add with
    // the same id must swap the texture, not leak, not drop it.
    std::ifstream bmp("./specs/assets/images/white8.bmp", std::ios::binary);
    Assert::That(static_cast<bool>(bmp), Equals(true));
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(bmp)),
                               std::istreambuf_iterator<char>());
    Assert::That(bytes.size() > 0u, Equals(true));

    PackWriter writer;
    Assert::That(writer.Add("white8.bmp", bytes), Equals(true));
    std::ostringstream out;
    Assert::That(writer.Save(out), Equals(true));
    std::istringstream in(out.str());
    PackReader pack;
    Assert::That(pack.Open(in), Equals(true));

    // Declared after the target so the store dies first: its destructor
    // destroys textures, and that needs a live renderer.
    SpecSurfaceTarget target(8, 8);
    AssetStore store;
    Assert::That(target.IsUsable(), Equals(true));
    store.AddTexture(target.renderer, "tex", pack, "white8.bmp");
    SDL_Texture *first = store.GetTexture("tex");
    Assert::That(first != nullptr, Equals(true));

    store.AddTexture(target.renderer, "tex", pack, "white8.bmp");
    SDL_Texture *second = store.GetTexture("tex");
    Assert::That(second != nullptr, Equals(true));
    Assert::That(second == first, Equals(false)); // replaced, not reused
  };

  It(should_not_store_a_font_when_the_pack_entry_is_missing) {
    std::ostringstream out;
    Assert::That(PackWriter().Save(out), Equals(true)); // valid, zero entries
    std::istringstream in(out.str());
    PackReader pack;
    Assert::That(pack.Open(in), Equals(true));

    AssetStore store;
    store.AddFont("font", pack, "no/such.ttf", 16);
    Assert::That(store.GetFont("font") == nullptr, Equals(true));
  };

  It(should_not_store_a_font_when_the_blob_is_undecodable) {
    PackWriter writer;
    Assert::That(writer.Add("junk.ttf", std::vector<uint8_t>{0, 1, 2, 3}),
                 Equals(true));
    std::ostringstream out;
    Assert::That(writer.Save(out), Equals(true));
    std::istringstream in(out.str());
    PackReader pack;
    Assert::That(pack.Open(in), Equals(true));

    AssetStore store;
    store.AddFont("font", pack, "junk.ttf", 16);
    Assert::That(store.GetFont("font") == nullptr, Equals(true));
  };

  It(should_not_store_a_sound_when_the_pack_entry_is_missing) {
    std::ostringstream out;
    Assert::That(PackWriter().Save(out), Equals(true)); // valid, zero entries
    std::istringstream in(out.str());
    PackReader pack;
    Assert::That(pack.Open(in), Equals(true));

    AssetStore store;
    store.AddSound("sfx", pack, "no/such.wav");
    Assert::That(store.GetSound("sfx") == nullptr, Equals(true));
  };

  It(should_not_store_a_sound_when_the_blob_is_undecodable) {
    PackWriter writer;
    Assert::That(writer.Add("junk.wav", std::vector<uint8_t>{9, 9, 9, 9}),
                 Equals(true));
    std::ostringstream out;
    Assert::That(writer.Save(out), Equals(true));
    std::istringstream in(out.str());
    PackReader pack;
    Assert::That(pack.Open(in), Equals(true));

    AssetStore store;
    store.AddSound("sfx", pack, "junk.wav");
    Assert::That(store.GetSound("sfx") == nullptr, Equals(true));
  };

  It(should_not_store_a_font_when_the_blob_is_empty) {
    PackWriter writer;
    Assert::That(writer.Add("empty.ttf", std::vector<uint8_t>{}), Equals(true));
    std::ostringstream out;
    Assert::That(writer.Save(out), Equals(true));
    std::istringstream in(out.str());
    PackReader pack;
    Assert::That(pack.Open(in), Equals(true));

    AssetStore store;
    store.AddFont("font", pack, "empty.ttf", 16);
    Assert::That(store.GetFont("font") == nullptr, Equals(true));
  };

  It(should_not_store_a_sound_when_the_blob_is_empty) {
    PackWriter writer;
    Assert::That(writer.Add("empty.wav", std::vector<uint8_t>{}), Equals(true));
    std::ostringstream out;
    Assert::That(writer.Save(out), Equals(true));
    std::istringstream in(out.str());
    PackReader pack;
    Assert::That(pack.Open(in), Equals(true));

    AssetStore store;
    store.AddSound("sfx", pack, "empty.wav");
    Assert::That(store.GetSound("sfx") == nullptr, Equals(true));
  };

  It(should_load_a_font_from_a_pack_and_still_render_after_the_call) {
    // The blob-lifetime spec. SDL_ttf reads glyphs lazily out of the RWops
    // it was opened with, so the blob behind a pack font must outlive the
    // font. Measure forces a glyph read long after AddFont's local vector
    // is gone; run the suite under valgrind (make -f Makefile.debian
    // memcheck TARGET=./bin/tests) and this is the test that catches a
    // regression to a borrowed blob.
    std::ifstream ttf("./specs/assets/fonts/font.ttf", std::ios::binary);
    Assert::That(static_cast<bool>(ttf), Equals(true));
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(ttf)),
                               std::istreambuf_iterator<char>());
    Assert::That(bytes.size() > 0u, Equals(true));

    PackWriter writer;
    Assert::That(writer.Add("font.ttf", bytes), Equals(true));
    std::ostringstream out;
    Assert::That(writer.Save(out), Equals(true));
    std::istringstream in(out.str());
    PackReader pack;
    Assert::That(pack.Open(in), Equals(true));

    Assert::That(TTF_Init() == 0, Equals(true));
    AssetStore store;
    store.AddFont("font", pack, "font.ttf", 16);
    TTF_Font *font = store.GetFont("font");
    Assert::That(font != nullptr, Equals(true));
    const SDL_Point size = Text::Measure(font, "AJ");
    Assert::That(size.x > 0, Equals(true));
    Assert::That(size.y > 0, Equals(true));
    store.ClearAssets(); // frees the font (and its RWops) before TTF_Quit
    TTF_Quit();
  };

  It(should_keep_the_old_texture_when_a_re_add_fails) {
    // The other half of the replace contract: a failed load on an id that
    // already holds an asset must keep the old one, not clobber or drop it.
    std::ifstream bmp("./specs/assets/images/white8.bmp", std::ios::binary);
    Assert::That(static_cast<bool>(bmp), Equals(true));
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(bmp)),
                               std::istreambuf_iterator<char>());

    PackWriter writer;
    Assert::That(writer.Add("white8.bmp", bytes), Equals(true));
    std::ostringstream out;
    Assert::That(writer.Save(out), Equals(true));
    std::istringstream in(out.str());
    PackReader pack;
    Assert::That(pack.Open(in), Equals(true));

    // Declared after the target so the store dies first: its destructor
    // destroys textures, and that needs a live renderer.
    SpecSurfaceTarget target(8, 8);
    AssetStore store;
    store.AddTexture(target.renderer, "tex", pack, "white8.bmp");
    SDL_Texture *first = store.GetTexture("tex");
    Assert::That(first != nullptr, Equals(true));

    store.AddTexture(target.renderer, "tex", pack, "no/such.png");
    Assert::That(store.GetTexture("tex") == first, Equals(true));
  };

  // ── LoadPack: one choke point for the whole game ────────────────────────
  //
  // The wiring contract (the consuming game's item): LoadPack once at
  // startup, and every path-based Add whose blob is IN the pack loads from
  // it, with the entry name being the path minus its leading "assets/".
  // Paths that the pack does not hold fall back to the loose file, so a
  // development tree with a partial pack still works and call sites never
  // change.

  static std::string WriteTestPack(const char *name) {
    std::ifstream bmp("./specs/assets/images/white8.bmp", std::ios::binary);
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(bmp)),
                               std::istreambuf_iterator<char>());
    PackWriter writer;
    writer.Add("gfx/white8.bmp", bytes);
    std::ostringstream out;
    writer.Save(out);
    std::string path = std::string("./specs/assets/") + name;
    std::ofstream file(path, std::ios::binary);
    file << out.str();
    return path;
  }

  It(should_load_from_the_pack_when_loadpack_holds_the_entry) {
    const std::string packPath = WriteTestPack("loadpack-test.pak");

    AssetStore store;
    Assert::That(store.LoadPack(packPath), Equals(true));
    SpecSurfaceTarget target(8, 8);
    store.AddTexture(target.renderer, "tex", "assets/gfx/white8.bmp");
    SDL_Texture *texture = store.GetTexture("tex");
    Assert::That(texture != nullptr, Equals(true));
    store.ClearAssets();
    std::remove(packPath.c_str());
  };

  It(should_fall_back_to_the_loose_file_when_the_pack_misses) {
    const std::string packPath = WriteTestPack("loadpack-test.pak");

    AssetStore store;
    Assert::That(store.LoadPack(packPath), Equals(true));
    SpecSurfaceTarget target(8, 8);
    // In the pack, but under a name the strip rule does not produce.
    store.AddTexture(target.renderer, "tex", "specs/assets/images/white8.bmp");
    SDL_Texture *texture = store.GetTexture("tex");
    Assert::That(texture != nullptr, Equals(true));
    store.ClearAssets();
    std::remove(packPath.c_str());
  };

  It(should_keep_the_loose_contract_when_loadpack_fails) {
    AssetStore store;
    Assert::That(store.LoadPack("./specs/assets/no-such-pack.pak"),
                 Equals(false));
    SpecSurfaceTarget target(8, 8);
    store.AddTexture(target.renderer, "tex",
                     "./specs/assets/images/white8.bmp");
    Assert::That(store.GetTexture("tex") != nullptr, Equals(true));
    store.ClearAssets();
  };

  It(should_replace_the_pack_when_loadpack_is_called_again) {
    const std::string packPath = WriteTestPack("loadpack-test.pak");
    AssetStore store;
    Assert::That(store.LoadPack(packPath), Equals(true));
    Assert::That(store.LoadPack(packPath), Equals(true));
    SpecSurfaceTarget target(8, 8);
    store.AddTexture(target.renderer, "tex", "assets/gfx/white8.bmp");
    Assert::That(store.GetTexture("tex") != nullptr, Equals(true));
    store.ClearAssets();
    std::remove(packPath.c_str());
  };

  It(should_load_fonts_from_the_pack_through_loadpack) {
    std::ifstream ttf("./specs/assets/fonts/font.ttf", std::ios::binary);
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(ttf)),
                               std::istreambuf_iterator<char>());
    PackWriter writer;
    writer.Add("fonts/font.ttf", bytes);
    std::ostringstream out;
    writer.Save(out);
    const std::string packPath = "./specs/assets/loadpack-font.pak";
    std::ofstream file(packPath, std::ios::binary);
    file << out.str();

    Assert::That(TTF_Init() == 0, Equals(true));
    AssetStore store;
    Assert::That(store.LoadPack(packPath), Equals(true));
    store.AddFont("font", "assets/fonts/font.ttf", 16);
    TTF_Font *font = store.GetFont("font");
    Assert::That(font != nullptr, Equals(true));
    // The lazy-read proof, through the pack this time: Measure reads glyphs
    // long after AddFont returned, from the blob the store kept alive.
    const SDL_Point size = Text::Measure(font, "AJ");
    Assert::That(size.x > 0, Equals(true));
    Assert::That(size.y > 0, Equals(true));
    store.ClearAssets(); // frees the font (and its RWops) before TTF_Quit
    TTF_Quit();
    std::remove(packPath.c_str());
  };
};
