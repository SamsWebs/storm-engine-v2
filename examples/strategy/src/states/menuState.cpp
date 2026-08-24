#include "menuState.h"

#include "../ui.h"
#include "overworldState.h"

const std::string MenuState::s_menuID = "MENU";

MenuState::MenuState(SDL_Renderer *renderer, int windowWidth, int windowHeight,
                     bool isDebugging, AssetStore *assetStore,
                     GameStateMachine *machine, Gamepad *gamepad,
                     world::Campaign *campaign, bool &isRunning)
    : renderer_{renderer}, windowWidth_{windowWidth},
      windowHeight_{windowHeight}, isDebugging_{isDebugging},
      assetStore_{assetStore}, machine_{machine}, gamepad_{gamepad},
      campaign_{campaign}, isRunning_{isRunning} {}

bool MenuState::onEnter() {
    selected_  = 0;
    leaving_   = false;
    enteredMs_ = SDL_GetTicks();
    logger_.Log("MenuState entered");
    return true;
}

bool MenuState::onExit() {
    logger_.Log("MenuState exited");
    return true;
}

void MenuState::processInput() {
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
        // Ignore auto-repeat. Holding a key would otherwise scroll the
        // selection once per repeat event rather than once per press.
        if (event.key.repeat) {
            continue;
        }

        switch (event.key.keysym.sym) {
        case SDLK_ESCAPE:
            isRunning_ = false;
            return;
        case SDLK_UP:
            selected_ = (selected_ + MENU_COUNT - 1) % MENU_COUNT;
            break;
        case SDLK_DOWN:
            selected_ = (selected_ + 1) % MENU_COUNT;
            break;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
        case SDLK_SPACE:
            if (selected_ == 0) {
                if (!leaving_) {
                    leaving_ = true;
                    campaign_->Reset();
                    machine_->changeState(new OverworldState(
                        renderer_, windowWidth_, windowHeight_, isDebugging_,
                        assetStore_, machine_, gamepad_, campaign_,
                        isRunning_));
                    // This state is off the stack and its deletion is pending.
                    // Touching any member after changeState is a use-after-free
                    // waiting to happen -- return immediately.
                    return;
                }
            } else {
                isRunning_ = false;
                return;
            }
            break;
        default:
            break;
        }
    }
}

void MenuState::update() {
    gamepad_->Update();
    if (leaving_) {
        return;
    }

    if (gamepad_->Pressed(GamepadButton::Up)) {
        selected_ = (selected_ + MENU_COUNT - 1) % MENU_COUNT;
    }
    if (gamepad_->Pressed(GamepadButton::Down)) {
        selected_ = (selected_ + 1) % MENU_COUNT;
    }
    if (gamepad_->Pressed(GamepadButton::Back)) {
        isRunning_ = false;
        return;
    }
    if (gamepad_->Pressed(GamepadButton::A) || gamepad_->Pressed(GamepadButton::Start)) {
        if (selected_ == 0) {
            leaving_ = true;
            campaign_->Reset();
            machine_->changeState(new OverworldState(
                renderer_, windowWidth_, windowHeight_, isDebugging_,
                assetStore_, machine_, gamepad_, campaign_, isRunning_));
            return;
        }
        isRunning_ = false;
        return;
    }
}

void MenuState::render() {
    SDL_SetRenderDrawColor(renderer_, 38, 64, 48, 255);
    SDL_RenderClear(renderer_);

    const int cx = windowWidth_ / 2;

    ui::DrawTextureCentred(renderer_, assetStore_->GetTexture("title"), cx, 120,
                           1.0f);

    SDL_Texture *items[MENU_COUNT] = {assetStore_->GetTexture("menu_start"),
                                      assetStore_->GetTexture("menu_quit")};

    for (int i = 0; i < MENU_COUNT; ++i) {
        const int y = 340 + i * 90;
        if (i == selected_) {
            // Own marker rather than a second copy of each label: two textures
            // per item would have to be regenerated together every time the
            // wording changed.
            int w = 0, h = 0;
            SDL_QueryTexture(items[i], nullptr, nullptr, &w, &h);
            ui::DrawPanel(renderer_, cx - w / 2 - 28, y - 12, w + 56, h + 24,
                          150);
        }
        ui::DrawTextureCentred(renderer_, items[i], cx, y, 1.0f);
    }

    ui::DrawTextureCentred(renderer_, assetStore_->GetTexture("hint_map"), cx,
                           windowHeight_ - 60, 1.0f);

    SDL_RenderPresent(renderer_);
}
