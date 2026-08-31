#pragma once

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include <SDL2/SDL.h>

#include <glm/glm.hpp>
#include <sol/sol.hpp>

#include <stormengine2/components/animation.h>
#include <stormengine2/components/boxCollider.h>
#include <stormengine2/components/sprite.h>
#include <stormengine2/components/transform.h>
#include <stormengine2/ecs.h>
#include <stormengine2/logger.h>

#include "../AssetManager.h"
#include "../Canvas.h"
#include "LuaWriter.h"
#include "Utilities.h"

using namespace storm;

class FileLoader {
private:
  void LoadMap(const AssetManager_Ptr &assetManager,
               const std::string &filename);
  void SaveMap(std::filesystem::path filename);
  void SaveColliders(std::filesystem::path filename);

  Logger logger;

public:
  FileLoader();
  ~FileLoader();

  void LoadProject(sol::state &lua, const std::string &filename,
                   const AssetManager_Ptr &assetManager, Renderer &renderer,
                   std::vector<std::string> &assetIds,
                   std::vector<std::string> &assetFilepaths,
                   std::shared_ptr<Canvas> &canvas, int &tileSize);

  void SaveProject(const std::string &filename,
                   std::vector<std::string> &assetIds,
                   std::vector<std::string> &assetFilepaths,
                   const int &canvasWidth, const int &canvasHeight,
                   const int &tileSize);

  void SaveToLuaTable(const std::string &filename,
                      std::vector<std::string> &assetIds,
                      std::vector<std::string> &assetFilepaths,
                      const int &tileSize);
};