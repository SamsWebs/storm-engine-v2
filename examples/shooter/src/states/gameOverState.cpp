#include "gameOverState.h"

#include "../ui.h"
#include "menuState.h"

namespace {
constexpr Uint32 AUTO_RETURN_MS = 6000; // or ENTER, whichever comes first
} // namespace

const std::string GameOverState::s_overID = "GAME_OVER_STATE";

GameOverState::GameOverState(SDL_Renderer *renderer, int windowWidth,
                             int windowHeight, bool isDebugging,
                             AssetStore *assetStore, GameStateMachine *machine,
                             Gamepad *gamepad, bool &isRunning, int finalScore,
                             int wavesSurvived)
    : renderer_(renderer), windowWidth_(windowWidth),
      windowHeight_(windowHeight), isDebugging_(isDebugging),
      assetStore_(assetStore), machine_(machine), gamepad_(gamepad),
      isRunning_(isRunning), finalScore_(finalScore),
      wavesSurvived_(wavesSurvived) {}

bool GameOverState::onEnter() {
  enteredAt_ = SDL_GetTicks();
  logger_.Log("GAME OVER -- final score " + std::to_string(finalScore_) +
              " after " + std::to_string(wavesSurvived_) + " waves");
  return true;
}

bool GameOverState::onExit() { return true; }

void GameOverState::ToMenu() {
  if (leaving_) {
    return; // changeState is deferred; do not queue it twice
  }
  leaving_ = true;
  machine_->changeState(new MenuState(renderer_, windowWidth_, windowHeight_,
                                      isDebugging_, assetStore_, machine_,
                                      gamepad_, isRunning_));
}

void GameOverState::processInput() {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    gamepad_->HandleEvent(event);
    if (event.type == SDL_QUIT) {
      isRunning_ = false;
      return;
    }
    // SDL delivers auto-repeat KEYDOWNs for a held key. SPACE is the fire
    // button during play and the transition here happens inside update(),
    // after that frame's queue was drained -- so without this guard the
    // still-held key dismisses GAME OVER within a frame or two.
    if (event.type != SDL_KEYDOWN || event.key.repeat) {
      continue;
    }
    if (event.key.keysym.sym == SDLK_ESCAPE) {
      isRunning_ = false;
      return;
    }
    if (event.key.keysym.sym == SDLK_RETURN ||
        event.key.keysym.sym == SDLK_KP_ENTER ||
        event.key.keysym.sym == SDLK_SPACE) {
      ToMenu();
      return; // this state is defunct now
    }
  }
}

void GameOverState::update() {
  gamepad_->Update();
  if (gamepad_->PressedBack()) {
    isRunning_ = false;
    return;
  }
  if (gamepad_->PressedA() || gamepad_->PressedStart()) {
    ToMenu();
    return;
  }

  SDL_Delay(MILLISECS_PER_FRAME);
  if (!leaving_ && SDL_GetTicks() - enteredAt_ >= AUTO_RETURN_MS) {
    ToMenu();
    return;
  }
}

void GameOverState::render() {
  SDL_SetRenderDrawColor(renderer_, 12, 8, 8, 255);
  SDL_RenderClear(renderer_);

  const int cx = windowWidth_ / 2;
  ui::DrawTextureCentred(renderer_, assetStore_->GetTexture("gameOver"), cx,
                         200, 3.0f);

  // SCORE: label and the number, centred as one unit.
  SDL_Texture *label = assetStore_->GetTexture("scoreLabel");
  SDL_Texture *digits = assetStore_->GetTexture("digits");
  int lw = 0, lh = 0;
  if (label) {
    SDL_QueryTexture(label, nullptr, nullptr, &lw, &lh);
  }
  const float s = 2.0f;
  const int nDigits = static_cast<int>(std::to_string(finalScore_).size());
  const int totalW = static_cast<int>(lw * s) + 10 +
                     nDigits * static_cast<int>(ui::DIGIT_W * s);
  const int startX = cx - totalW / 2;
  ui::DrawTexture(renderer_, label, startX, 300, s);
  ui::DrawNumber(renderer_, digits, finalScore_,
                 startX + static_cast<int>(lw * s) + 10, 302, s);

  SDL_RenderPresent(renderer_);
}
