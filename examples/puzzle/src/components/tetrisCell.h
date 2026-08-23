#pragma once

// Attached to locked board cells and to the 4 cells of the active piece.
struct TetrisCellComponent {
  int boardRow = 0;
  int boardCol = 0;
  int colorType = 0;     // 1-7 matching piece type + 1
  bool isActive = false; // true = part of the falling piece

  TetrisCellComponent() = default;
  TetrisCellComponent(int row, int col, int color, bool active)
      : boardRow{row}, boardCol{col}, colorType{color}, isActive{active} {}
};
