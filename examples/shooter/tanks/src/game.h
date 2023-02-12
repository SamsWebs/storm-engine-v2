#pragma once

#include <SDL2/SDL.h>

#include <stormengine2/logger.h>

const int FPS = 60;
const int MILLISECS_PER_FRAME = 1000 / FPS;

class Game {
public:
  Game();
  ~Game();
  void Initialize();
  void Run();
  void Setup();
  void ProcessInput();
  void Update();
  void Render();
  void Destroy();

  int windowWidth;
  int windowHeight;

private:
  bool isRunning;
  int millisecsPreviousFrame = 0;
  SDL_Window *window;
  SDL_Renderer *renderer;
  Logger logger;
};
