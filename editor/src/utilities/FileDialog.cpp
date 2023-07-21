#include "FileDialog.h"

std::string FileDialog::OpenFile() { return FileDialogWin::OpenFile(); }

std::string FileDialog::SaveFile() { return FileDialogWin::SaveFile(); }

std::string FileDialog::OpenImageFile() {
  return FileDialogWin::OpenImageFile();
}
