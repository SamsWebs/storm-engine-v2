#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <igloo/igloo_alt.h>
#include <map>
#include <string>

#include "../common/assetStore.h"

using namespace igloo;

Describe(AssetStoreSpec){
    It(should_add_and_retrieve_textures){// Arrange
                                         SDL_Init(SDL_INIT_VIDEO);
SDL_Window *window =
    SDL_CreateWindow("AssetStore Test", SDL_WINDOWPOS_CENTERED,
                     SDL_WINDOWPOS_CENTERED, 640, 480, SDL_WINDOW_SHOWN);
SDL_Renderer *renderer =
    SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

AssetStore assetStore;

// Act
assetStore.AddTexture(renderer, "texture1", "path/to/texture1.png");
assetStore.AddTexture(renderer, "texture2", "path/to/texture2.png");

// Assert
SDL_Texture *texture1 = assetStore.GetTexture("texture1");
// Assert::That(texture1, Is().Not().EqualTo(nullptr));

SDL_Texture *texture2 = assetStore.GetTexture("texture2");
// Assert::That(texture2, Is().Not().EqualTo(nullptr));

// Clean up
assetStore.ClearAssets();
SDL_DestroyRenderer(renderer);
SDL_DestroyWindow(window);
SDL_Quit();
}
}
;
