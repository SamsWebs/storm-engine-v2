#include "game.h"

#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>

#include "states/playState.h"

Game::Game() { assetStore_ = std::make_unique<AssetStore>(); }

void Game::Initialize() {
  SDL_Init(SDL_INIT_EVERYTHING);
  IMG_Init(IMG_INIT_PNG);
  TTF_Init();

  window_ = SDL_CreateWindow("My Storm Game", SDL_WINDOWPOS_CENTERED,
                             SDL_WINDOWPOS_CENTERED, windowWidth_,
                             windowHeight_, SDL_WINDOW_SHOWN);
  renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED);

  // The machine takes ownership of the state - never delete it yourself.
  // isRunning_ goes in by reference because the engine has no quit API: a
  // state stops the loop by writing to it.
  stateMachine_.changeState(new PlayState(renderer_, windowWidth_,
                                          windowHeight_, std::move(assetStore_),
                                          isRunning_));
  isRunning_ = true;
}

void Game::Run() {
  Initialize();
  while (isRunning_) {
    // The active state owns ALL event polling. Do not call SDL_PollEvent here
    // as well - the queue is shared, whoever polls first consumes it, and the
    // other silently sees no input.
    stateMachine_.processInput();
    stateMachine_.update();
    stateMachine_.render();
  }
}

void Game::Destroy() {
  stateMachine_.clean();
  SDL_DestroyRenderer(renderer_);
  SDL_DestroyWindow(window_);
  TTF_Quit();
  IMG_Quit();
  SDL_Quit();
}
