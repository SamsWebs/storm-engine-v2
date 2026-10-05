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
                             InputHub *input, bool &isRunning, int finalScore,
                             int wavesSurvived)
    : renderer_(renderer), windowWidth_(windowWidth),
      windowHeight_(windowHeight), isDebugging_(isDebugging),
      assetStore_(assetStore), machine_(machine), input_(input),
      isRunning_(isRunning), finalScore_(finalScore),
      wavesSurvived_(wavesSurvived) {
  ActionBinding accept;
  accept.key = SDL_SCANCODE_RETURN;
  accept.pad = GamepadButton::A;
  actions_.Bind(static_cast<int>(Action::Accept), accept);

  // Its own action id rather than a second bind of Accept: Bind replaces the
  // binding for an id instead of adding to it.
  ActionBinding acceptKeypadEnter;
  acceptKeypadEnter.key = SDL_SCANCODE_KP_ENTER;
  actions_.Bind(static_cast<int>(Action::AcceptKeypadEnter), acceptKeypadEnter);

  ActionBinding back;
  back.key = SDL_SCANCODE_ESCAPE;
  back.pad = GamepadButton::Back;
  actions_.Bind(static_cast<int>(Action::Back), back);

  input_->RegisterMap(&actions_);
}

bool GameOverState::onEnter() {
  enteredAt_ = SDL_GetTicks();
  logger_.Log("GAME OVER -- final score " + std::to_string(finalScore_) +
              " after " + std::to_string(wavesSurvived_) + " waves");
  return true;
}

bool GameOverState::onExit() {
  input_->UnregisterMap(&actions_);
  return true;
}

void GameOverState::ToMenu() {
  if (leaving_) {
    return; // changeState is deferred; do not queue it twice
  }
  leaving_ = true;
  machine_->changeState(new MenuState(renderer_, windowWidth_, windowHeight_,
                                      isDebugging_, assetStore_, machine_,
                                      input_, isRunning_));
}

void GameOverState::processInput() {
  // No poll here any more -- the hub owns that, once, for the whole process.
  if (actions_.WasPressed(static_cast<int>(Action::Back))) {
    isRunning_ = false;
  }
}

void GameOverState::update() {
  if (actions_.WasPressed(static_cast<int>(Action::Back))) {
    isRunning_ = false;
    return;
  }
  if (actions_.WasPressed(static_cast<int>(Action::Accept)) ||
      actions_.WasPressed(static_cast<int>(Action::AcceptKeypadEnter))) {
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
