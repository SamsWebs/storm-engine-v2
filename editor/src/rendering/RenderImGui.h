#pragma once

#include <memory>

#include <SDL2/SDL.h>

#include <imgui/imgui.h>
#include <imgui/imgui_sdl.h>
#include <imgui/imgui_stdlib.h>
#include <sol/sol.hpp>

#include <stormengine2/ecs.h>

#include "../AssetManager.h"
#include "../Canvas.h"
#include "../MouseControl.h"
#include "../utilities/AddTileCommand.h"
#include "../utilities/ChangeCanvasSizeCommand.h"
#include "../utilities/CommandManager.h"
#include "../utilities/RemoveTileCommand.h"
#include "../utilities/Utilities.h"
#include "IconsFontAwesome.h"
#include "ImGuiFunc.h"

using namespace storm;

class RenderGuiSystem : public System {
private:
  std::unique_ptr<class ImGuiFuncs> mImFuncs;
  std::shared_ptr<class MouseControl> mMouseControl;
  std::shared_ptr<class Canvas> mCanvas;

  std::string mWindowName;

  bool mCreateTiles, mCreateColliders, mGridSnap, mExit, mTileRemoved;
  int mCanvasWidth, mPrevCanvasWidth, mCanvasHeight, mPrevCanvasHeight,
      mTileSize, mPrevTileSize, mGridX, mGridY;

  sol::state mLua;
  std::unique_ptr<class CommandManager> mCommandManager;

  static constexpr int EDITOR_W = 1280;
  static constexpr int EDITOR_H = 720 - 25; // matches Application WINDOW_HEIGHT

private:
  void SetExit(bool exit) { mExit = exit; }
  void ShowMouseLocationText(SDL_Rect &mouseBox, SDL_Rect &camera);
  const bool MouseOffCanvas() const;
  void UpdateCanvas();

public:
  RenderGuiSystem();
  ~RenderGuiSystem();

  void Update(const AssetManager_Ptr &assetManager, Renderer &renderer,
              SDL_Rect &mouseBox, SDL_Rect &camera, SDL_Event &event,
              const float &zoom, const float &dt);

  /*
          RenderGrid: This renders a grid on the screen based on the Canvas
     Width/Height and the tile size. [gridCols] = canvasWidth / tilesize
     [gridRows] = canvasHeight / tilesize This also works with the zoom and pan
     functionality.
  */
  void RenderGrid(Renderer &renderer, SDL_Rect &camera, const float &zoom);

  /*
   *	CreateNewCanvas() - This function erases all tiles and removes them from
   *the Registry::Instance() It also resets all settings back to default,
   *starting a new fresh canvas
   */
  void CreateNewCanvas();

  /*
   *	SetWindowName - Sets the Title for the main SDL Window. This will change
   *based on if the project has been loaded or saved. The name of the project
   *will be in the title.
   */
  inline void SetWindowName(SDLWindowPtr &window) {
    SDL_SetWindowTitle(window.get(), mWindowName.c_str());
  }

  /*
   *	GetExit() - Exit the application
   */
  inline const bool &GetExit() const { return mExit; }

  // Re-centre the camera so the canvas sits in the middle of the viewport.
  void CenterCamera(SDL_Rect &camera) const {
    camera.x = -(EDITOR_W - mCanvasWidth)  / 2;
    camera.y = -(EDITOR_H - mCanvasHeight) / 2;
  }
};