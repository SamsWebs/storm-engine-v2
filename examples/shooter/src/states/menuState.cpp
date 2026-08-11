#include "menuState.h"

#include "playState.h"
#include "../ui.h"

namespace {
// ui_menu.png is one image containing all five options; lines are ~13px tall
// on a ~17px pitch (assets/SHEET.md). The '>' cursor is baked into the first
// line, so selection on any other line needs a marker of our own.
constexpr int MENU_LINE_H     = 13;
constexpr int MENU_LINE_PITCH = 17;
constexpr int MENU_COUNT      = 2;   // PLAY GAME, QUIT
} // namespace

const std::string MenuState::s_menuID = "MENU_STATE";

MenuState::MenuState(SDL_Renderer *renderer, int windowWidth, int windowHeight,
                     bool isDebugging, AssetStore *assetStore,
                     GameStateMachine *machine, Gamepad *gamepad, bool &isRunning)
    : renderer_(renderer), windowWidth_(windowWidth),
      windowHeight_(windowHeight), isDebugging_(isDebugging),
      assetStore_(assetStore), machine_(machine), gamepad_(gamepad),
      isRunning_(isRunning) {}

bool MenuState::onEnter() {
    // Systems before entities: AddSystem never scans existing entities, so one
    // registered afterwards starts empty and stays empty.
    registry_.AddSystem<MovementSystem>();
    registry_.AddSystem<RenderSystem>();

    // Attract-mode planes drifting down behind the menu. They also mean the
    // menu has live entities, which is what verify.sh checks for.
    SpawnAttractPlane(90.0f,  -40.0f,  35.0f, 2);
    SpawnAttractPlane(210.0f, -180.0f, 45.0f, 4);
    SpawnAttractPlane(610.0f, -90.0f,  40.0f, 2);
    SpawnAttractPlane(700.0f, -260.0f, 30.0f, 4);

    // Flush now: creation is deferred, and the first render() after a
    // changeState would otherwise show an empty screen for one frame.
    registry_.Update();

    lastFrame_ = SDL_GetTicks();
    return true;
}

// resume() is called when a pushed state pops. It must NOT re-run onEnter():
// that would rebuild the attract planes and leak the first set.
void MenuState::resume() { lastFrame_ = SDL_GetTicks(); }

bool MenuState::onExit() { return true; }   // Game owns the assets; do not clear

void MenuState::SpawnAttractPlane(float x, float y, float speed, int row) {
    Entity e = registry_.CreateEntity();
    e.Group("attract");
    e.AddComponent<TransformComponent>(glm::vec2(x, y), glm::vec2(1, 1), 0.0);
    e.AddComponent<RigidBodyComponent>(glm::vec2(0, speed));
    e.AddComponent<SpriteComponent>("sheet", 32, 32, 0, false, 0, row * 32);
    // No flip: the sheet's aircraft are drawn nose-down, which is the way
    // these attract-mode planes travel.
}

void MenuState::processInput() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        gamepad_->HandleEvent(event);   // device add/remove only
        if (event.type == SDL_QUIT) {
            isRunning_ = false;
            return;
        }
        // Ignore auto-repeat: holding an arrow would otherwise scroll the
        // selection every frame, and a held ENTER arriving from the previous
        // state would confirm immediately.
        if (event.type != SDL_KEYDOWN || event.key.repeat) {
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
            if (selected_ == 0) {           // PLAY GAME
                machine_->changeState(new PlayState(
                    renderer_, windowWidth_, windowHeight_, isDebugging_,
                    assetStore_, machine_, gamepad_, isRunning_));
                return;                     // this state is defunct now
            }
            if (selected_ == 1) {           // QUIT
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
    // Sample the pad once per frame. Edge-triggered so holding a direction
    // does not scroll the menu at 60 entries a second.
    gamepad_->Update();
    if (gamepad_->PressedUp()) {
        selected_ = (selected_ + MENU_COUNT - 1) % MENU_COUNT;
    }
    if (gamepad_->PressedDown()) {
        selected_ = (selected_ + 1) % MENU_COUNT;
    }
    if (gamepad_->PressedBack()) {
        isRunning_ = false;
        return;
    }
    if (gamepad_->PressedA() || gamepad_->PressedStart()) {
        if (selected_ == 0) {
            machine_->changeState(new PlayState(
                renderer_, windowWidth_, windowHeight_, isDebugging_,
                assetStore_, machine_, gamepad_, isRunning_));
            return;                     // this state is defunct now
        }
        if (selected_ == 1) {
            isRunning_ = false;
            return;
        }
    }

    int wait = MILLISECS_PER_FRAME - (SDL_GetTicks() - lastFrame_);
    if (wait > 0 && wait <= MILLISECS_PER_FRAME) {
        SDL_Delay(wait);
    }
    const double dt = (SDL_GetTicks() - lastFrame_) / 1000.0;
    lastFrame_ = SDL_GetTicks();

    registry_.Update();   // flush deferred adds/kills before any system runs
    registry_.GetSystem<MovementSystem>().Update(dt);

    // Recycle the attract planes rather than creating new ones, so the menu
    // can sit on screen indefinitely without growing the entity pools.
    if (registry_.DoesGroupExist("attract")) {
        for (auto &e : registry_.GetEntitiesByGroup("attract")) {
            auto *tf = registry_.TryGetComponent<TransformComponent>(e);
            if (tf && tf->position.y > windowHeight_ + 32.0f) {
                tf->position.y = -32.0f - (tf->position.x * 0.3f);
            }
        }
    }
}

void MenuState::render() {
    // Sky blue, sampled from the 1945 logo so the panel sits on its own
    // background rather than a black void.
    SDL_SetRenderDrawColor(renderer_, 0, 99, 191, 255);
    SDL_RenderClear(renderer_);

    registry_.GetSystem<RenderSystem>().Update(renderer_, *assetStore_);

    const int cx = windowWidth_ / 2;

    SDL_Texture *logo = assetStore_->GetTexture("logo");
    SDL_Texture *menu = assetStore_->GetTexture("menu");
    int lw = 0, lh = 0, mw = 0, mh = 0;
    if (logo) {
        SDL_QueryTexture(logo, nullptr, nullptr, &lw, &lh);
    }
    if (menu) {
        SDL_QueryTexture(menu, nullptr, nullptr, &mw, &mh);
    }

    // Lay the logo and the menu out as one block and centre that block, rather
    // than pinning each to a hardcoded y. Changing either scale now keeps the
    // composition balanced instead of drifting off-centre.
    const float logoScale = 3.5f;
    const float scale     = 2.0f;
    const int   gap       = 60;
    const int   blockH    = static_cast<int>(lh * logoScale) + gap +
                            static_cast<int>(mh * scale);
    const int   blockTop  = (windowHeight_ - blockH) / 2;

    ui::DrawTextureCentred(renderer_, logo, cx, blockTop, logoScale);

    const int menuX = cx - static_cast<int>(mw * scale) / 2;
    const int menuY = blockTop + static_cast<int>(lh * logoScale) + gap;
    ui::DrawTexture(renderer_, menu, menuX, menuY, scale);

    // The baked-in '>' only marks PLAY GAME. Draw our own marker beside the
    // selected line instead of trying to move a cursor that is part of the art.
    const int lineY = menuY + static_cast<int>(selected_ * MENU_LINE_PITCH * scale);
    SDL_Rect  marker{menuX - 22, lineY + 4,
                     14, static_cast<int>(MENU_LINE_H * scale) - 8};
    SDL_SetRenderDrawColor(renderer_, 255, 200, 40, 255);
    SDL_RenderFillRect(renderer_, &marker);

    SDL_RenderPresent(renderer_);
}
