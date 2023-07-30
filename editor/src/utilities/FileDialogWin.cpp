#include "FileDialogWin.h"

std::string FileDialogWin::OpenFile(const char *filter) {
  std::string filePath;

  nfdchar_t *outPath = nullptr;
  nfdresult_t result = NFD_OpenDialog(filter, nullptr, &outPath);
  if (result == NFD_OKAY) {
    filePath = outPath;
    free(outPath);
  } else if (result == NFD_ERROR) {
    // Handle error if needed
  }

  return filePath;
}

std::string FileDialogWin::SaveFile(const char *filter) {
  std::string filePath;

  nfdchar_t *outPath = nullptr;
  nfdresult_t result = NFD_SaveDialog(filter, nullptr, &outPath);
  if (result == NFD_OKAY) {
    filePath = outPath;
    free(outPath);
  } else if (result == NFD_ERROR) {
    // Handle error if needed
  }

  return filePath;
}

std::string FileDialogWin::OpenImageFile(const char *filter) {
  return OpenFile(filter);
}
