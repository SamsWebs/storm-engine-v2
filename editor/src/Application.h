#pragma once

#include <cmath>
#include <memory>

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>

#include <glm/glm.hpp>
#include <imgui/imgui.h>
#include <imgui/imgui_impl_sdl.h>
#include <imgui/imgui_sdl.h>
#include <imgui/imgui_stdlib.h>
#include <sol/sol.hpp>

#include <stormengine2/ecs.h>
#include <stormengine2/logger.h>

#include "./utilities/Utilities.h"
#include "AssetManager.h"
#include "rendering/AnimationSystem.h"
#include "rendering/IconsFontAwesome.h"
#include "rendering/RenderCollisionSystem.h"
#include "rendering/RenderImGui.h"
#include "rendering/RenderSystem.h"

class Application {

private:
  const int WINDOW_BAR = 25;
  const int WINDOW_HEIGHT = 1080 - WINDOW_BAR;
  const int WINDOW_WIDTH = 1920;

  const int DEFAULT_ZOOM = 1;
  const int DEFAULT_CAM_X = -360;
  const int DEFAULT_CAM_Y = -176;

  const int CAM_SPEED = 10;

  SDLWindowPtr mWindow;
  Renderer mRenderer;

  SDL_Rect mCamera;
  SDL_Rect mMouseBox;
  SDL_Event mEvent;

  float mDeltaTime;
  bool mIsRunning, mShowColliders;

  int mMsPrevFrame, mMsPerFrame;

  float mZoom;

  AssetManager_Ptr mAssetManager;

  // Define constants
  const int FPS = 60;
  const int MILLISECONDS_PER_FRAME = 1000 / FPS;

private:
  void Init();
  void Draw();
  void ProcessEvents();
  void Update();
  void UpdateDeltaTime();
  void CameraControl(SDL_Event &event);
  void Zoom(SDL_Event &event);

  Logger logger;

public:
  Application();
  ~Application();

  void Run();
  void ShutDown();
};