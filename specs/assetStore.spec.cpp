#include <igloo/igloo_alt.h>

#include "../common/assetStore.h"

using namespace igloo;

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
};
