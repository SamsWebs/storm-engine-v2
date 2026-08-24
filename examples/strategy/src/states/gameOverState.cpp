#include "gameOverState.h"

#include "../ui.h"
#include "menuState.h"

const std::string GameOverState::s_gameOverID = "GAMEOVER";

GameOverState::GameOverState(SDL_Renderer *renderer, int windowWidth,
                             int windowHeight, bool isDebugging,
                             AssetStore *assetStore, GameStateMachine *machine,
                             Gamepad *gamepad, world::Campaign *campaign,
                             world::Owner winner, bool &isRunning)
    : renderer_{renderer}, windowWidth_{windowWidth},
      windowHeight_{windowHeight}, isDebugging_{isDebugging},
      assetStore_{assetStore}, machine_{machine}, gamepad_{gamepad},
      campaign_{campaign}, winner_{winner}, isRunning_{isRunning} {}

bool GameOverState::onEnter() {
    enteredMs_ = SDL_GetTicks();
    leaving_   = false;
    logger_.Log("GameOverState entered");
    return true;
}

bool GameOverState::onExit() {
    logger_.Log("GameOverState exited");
    return true;
}

void GameOverState::ReturnToMenu() {
    if (leaving_) {
        return;
    }
    leaving_ = true;
    machine_->changeState(new MenuState(renderer_, windowWidth_, windowHeight_,
                                        isDebugging_, assetStore_, machine_,
                                        gamepad_, campaign_, isRunning_));
}

void GameOverState::processInput() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        gamepad_->HandleEvent(event);

        if (event.type == SDL_QUIT) {
            isRunning_ = false;
            return;
        }
        if (event.type != SDL_KEYDOWN) {
            continue;
        }
        // Auto-repeat would dismiss this screen the instant it appeared if the
        // player were still holding the key that ended the campaign.
        if (event.key.repeat) {
            continue;
        }
        if (event.key.keysym.sym == SDLK_ESCAPE) {
            isRunning_ = false;
            return;
        }
        ReturnToMenu();
        return;
    }
}

void GameOverState::update() {
    gamepad_->Update();
    if (leaving_) {
        return;
    }
    if (gamepad_->Pressed(GamepadButton::A) || gamepad_->Pressed(GamepadButton::Start)) {
        ReturnToMenu();
        return;
    }
    if (gamepad_->Pressed(GamepadButton::Back)) {
        isRunning_ = false;
        return;
    }
    if (SDL_GetTicks() - enteredMs_ > AUTO_RETURN_MS) {
        ReturnToMenu();
    }
}

void GameOverState::render() {
    const bool won = (winner_ == world::Owner::Blue);
    if (won) {
        SDL_SetRenderDrawColor(renderer_, 38, 64, 48, 255);
    } else {
        SDL_SetRenderDrawColor(renderer_, 60, 34, 38, 255);
    }
    SDL_RenderClear(renderer_);

    const int cx = windowWidth_ / 2;
    ui::DrawTextureCentred(renderer_,
                           assetStore_->GetTexture(won ? "win" : "lose"), cx,
                           260, 1.0f);

    ui::DrawPanel(renderer_, cx - 150, 400, 300, 60);
    ui::DrawTexture(renderer_, assetStore_->GetTexture("day"), cx - 128, 412,
                    0.9f);
    ui::DrawNumber(renderer_, assetStore_->GetTexture("digits"), campaign_->day,
                   cx - 40, 410, 0.9f);

    SDL_RenderPresent(renderer_);
}
