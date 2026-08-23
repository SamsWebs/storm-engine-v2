#pragma once

#include "../components/tetrisCell.h"
#include <stormengine2/components/transform.h>
#include <stormengine2/ecs.h>

// Keeps the TransformComponent of every board cell in sync with its
// TetrisCellComponent board position. Run this after any move/lock/shift so
// the existing RenderSystem sees up-to-date screen positions.
class TetrisSyncSystem : public System {
public:
  static constexpr int CELL = 32;
  static constexpr int BOARD_W = 10;
  static constexpr int BOARD_H = 20;

  int boardOffX = 0;
  int boardOffY = 0;

  TetrisSyncSystem() {
    RequireComponent<TetrisCellComponent>();
    RequireComponent<TransformComponent>();
  }

  void Update() {
    for (auto &entity : GetSystemEntities()) {
      const auto &cell = entity.GetComponent<TetrisCellComponent>();
      auto &transform = entity.GetComponent<TransformComponent>();
      transform.position.x = boardOffX + cell.boardCol * CELL;
      transform.position.y = boardOffY + cell.boardRow * CELL;
    }
  }
};
