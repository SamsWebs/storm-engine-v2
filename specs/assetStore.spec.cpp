#include <igloo/igloo_alt.h>

#include "../common/assetStore.h"

using namespace igloo;
using namespace storm;

// Headless specs — no window/renderer needed. They pin the store's error
// contract: lookups and failed loads yield nullptr, never a throw or a
// silently stored null texture.
Describe(AssetStoreSpec) {

  It(should_return_nullptr_for_a_missing_id) {
    AssetStore store;
    Assert::That(store.GetTexture("does-not-exist") == nullptr, Equals(true));
  };

  It(should_not_store_a_texture_when_the_file_fails_to_load) {
    AssetStore store;
    store.AddTexture(nullptr, "bad", "path/that/does/not/exist.png");
    Assert::That(store.GetTexture("bad") == nullptr, Equals(true));
  };

  It(should_be_safe_to_clear_an_empty_store) {
    AssetStore store;
    store.ClearAssets();
    Assert::That(store.GetTexture("anything") == nullptr, Equals(true));
  };

  It(should_return_nullptr_for_a_missing_font_id) {
    AssetStore store;
    Assert::That(store.GetFont("no-such-font") == nullptr, Equals(true));
  };

  It(should_not_store_a_font_when_the_file_fails_to_open) {
    AssetStore store;
    store.AddFont("bad", "path/that/does/not/exist.ttf", 16);
    Assert::That(store.GetFont("bad") == nullptr, Equals(true));
  };

  It(should_return_nullptr_for_a_missing_sound_id) {
    AssetStore store;
    Assert::That(store.GetSound("no-such-sound") == nullptr, Equals(true));
  };

  It(should_not_store_a_sound_when_the_file_fails_to_load) {
    AssetStore store;
    store.AddSound("bad", "path/that/does/not/exist.wav");
    Assert::That(store.GetSound("bad") == nullptr, Equals(true));
  };

  It(should_clear_fonts_and_sounds_as_well_as_textures) {
    // A store cleared twice must not double-free. ClearAssets empties each
    // container, so the second pass has nothing to close.
    AssetStore store;
    store.ClearAssets();
    store.ClearAssets();
    Assert::That(store.GetFont("anything") == nullptr, Equals(true));
    Assert::That(store.GetSound("anything") == nullptr, Equals(true));
  };
};
