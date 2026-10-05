#include "menuState.h"

#include "../ui.h"
#include "playState.h"

namespace {
// ui_menu.png is one image containing all five options; lines are ~13px tall
// on a ~17px pitch (assets/SHEET.md). The '>' cursor is baked into the first
// line, so selection on any other line needs a marker of our own.
constexpr int MENU_LINE_H = 13;
constexpr int MENU_LINE_PITCH = 17;
constexpr int MENU_COUNT = 2; // PLAY GAME, QUIT
} // namespace

const std::string MenuState::s_menuID = "MENU_STATE";

MenuState::MenuState(SDL_Renderer *renderer, int windowWidth, int windowHeight,
                     bool isDebugging, AssetStore *assetStore,
                     GameStateMachine *machine, InputHub *input,
                     bool &isRunning)
    : renderer_(renderer), windowWidth_(windowWidth),
      windowHeight_(windowHeight), isDebugging_(isDebugging),
      assetStore_(assetStore), machine_(machine), input_(input),
      isRunning_(isRunning) {
  // Bind here, not in onEnter: a rebind resets the map's edge state, and doing
  // it once in the constructor means a screen that is entered, left and entered
  // again cannot arrive with a stale "was down" from the previous visit.
  ActionBinding up;
  up.key = SDL_SCANCODE_UP;
  up.pad = GamepadButton::Up;
  actions_.Bind(static_cast<int>(Action::Up), up);

  ActionBinding down;
  down.key = SDL_SCANCODE_DOWN;
  down.pad = GamepadButton::Down;
  actions_.Bind(static_cast<int>(Action::Down), down);

  ActionBinding accept;
  accept.key = SDL_SCANCODE_RETURN;
  accept.pad = GamepadButton::A;
  actions_.Bind(static_cast<int>(Action::Accept), accept);

  // RETURN and KP_ENTER are the same physical key on most layouts but not on
  // every one, so keypad enter needs a binding of its own. It is a SEPARATE
  // action id, not a second binding of Accept: Bind REPLACES the binding for an
  // id rather than adding to it, so binding it twice would silently drop the
  // RETURN key and the pad's A button, leaving only the keypad key working.
  ActionBinding acceptKeypadEnter;
  acceptKeypadEnter.key = SDL_SCANCODE_KP_ENTER;
  actions_.Bind(static_cast<int>(Action::AcceptKeypadEnter), acceptKeypadEnter);

  ActionBinding back;
  back.key = SDL_SCANCODE_ESCAPE;
  back.pad = GamepadButton::Back;
  actions_.Bind(static_cast<int>(Action::Back), back);

  input_->RegisterMap(&actions_);
}

bool MenuState::onEnter() {
  // Systems before entities: AddSystem never scans existing entities, so one
  // registered afterwards starts empty and stays empty.
  registry_.AddSystem<MovementSystem>();
  registry_.AddSystem<RenderSystem>();

  // Attract-mode planes drifting down behind the menu. They also mean the
  // menu has live entities, which is what verify.sh checks for.
  SpawnAttractPlane(90.0f, -40.0f, 35.0f, 2);
  SpawnAttractPlane(210.0f, -180.0f, 45.0f, 4);
  SpawnAttractPlane(610.0f, -90.0f, 40.0f, 2);
  SpawnAttractPlane(700.0f, -260.0f, 30.0f, 4);

  // Flush now: creation is deferred, and the first render() after a
  // changeState would otherwise show an empty screen for one frame.
  registry_.Update();

  millisecondsPreviousFrame = SDL_GetTicks();
  return true;
}

// resume() is called when a pushed state pops. It must NOT re-run onEnter():
// that would rebuild the attract planes and leak the first set.
void MenuState::resume() { millisecondsPreviousFrame = SDL_GetTicks(); }

bool MenuState::onExit() {
  // Stop feeding a screen that is no longer on top. Leaving it registered would
  // keep updating a map nobody reads, and would leave a stale entry in the
  // hub's list for the life of the process.
  input_->UnregisterMap(&actions_);
  return true; // Game owns the assets; do not clear
}

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
  // No SDL_PollEvent here any more. Game::Run drains the queue once per frame
  // through the hub and updates every registered map before any state runs, so
  // this reads THIS frame's edges.
  if (actions_.WasPressed(static_cast<int>(Action::Back))) {
    isRunning_ = false;
  }
}

void MenuState::update() {
  // The pad is sampled by the hub's Poll(), once per frame, for the whole
  // process. This state no longer owns that -- which is why a state pushed
  // under the menu still receives events.
  if (actions_.WasPressed(static_cast<int>(Action::Up))) {
    selected_ = (selected_ + MENU_COUNT - 1) % MENU_COUNT;
  }
  if (actions_.WasPressed(static_cast<int>(Action::Down))) {
    selected_ = (selected_ + 1) % MENU_COUNT;
  }
  if (actions_.WasPressed(static_cast<int>(Action::Back))) {
    isRunning_ = false;
    return;
  }
  if (actions_.WasPressed(static_cast<int>(Action::Accept)) ||
      actions_.WasPressed(static_cast<int>(Action::AcceptKeypadEnter))) {
    if (selected_ == 0) {
      machine_->changeState(
          new PlayState(renderer_, windowWidth_, windowHeight_, isDebugging_,
                        assetStore_, machine_, input_, isRunning_));
      return; // this state is defunct now
    }
    if (selected_ == 1) {
      isRunning_ = false;
      return;
    }
  }

  // 0 keeps the delta unclamped, which is what this state has always done.
  const double dt = CapFrameRate(0.0);

  registry_.Update(); // flush deferred adds/kills before any system runs
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
  const float scale = 2.0f;
  const int gap = 60;
  const int blockH =
      static_cast<int>(lh * logoScale) + gap + static_cast<int>(mh * scale);
  const int blockTop = (windowHeight_ - blockH) / 2;

  ui::DrawTextureCentred(renderer_, logo, cx, blockTop, logoScale);

  const int menuX = cx - static_cast<int>(mw * scale) / 2;
  const int menuY = blockTop + static_cast<int>(lh * logoScale) + gap;
  ui::DrawTexture(renderer_, menu, menuX, menuY, scale);

  // The baked-in '>' only marks PLAY GAME. Draw our own marker beside the
  // selected line instead of trying to move a cursor that is part of the art.
  const int lineY =
      menuY + static_cast<int>(selected_ * MENU_LINE_PITCH * scale);
  SDL_Rect marker{menuX - 22, lineY + 4, 14,
                  static_cast<int>(MENU_LINE_H * scale) - 8};
  SDL_SetRenderDrawColor(renderer_, 255, 200, 40, 255);
  SDL_RenderFillRect(renderer_, &marker);

  SDL_RenderPresent(renderer_);
}
