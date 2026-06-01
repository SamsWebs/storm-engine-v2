#include "FileDialogWin.h"
#include <filesystem>

// Definition of the static member — starts empty (NFD will use its own default)
std::string FileDialogWin::sLastDirectory = "";

std::string FileDialogWin::DirectoryOf(const std::string &path) {
  std::filesystem::path p(path);
  return p.parent_path().string();
}

std::string FileDialogWin::OpenFile(const char *filter) {
  std::string filePath;

  nfdchar_t *outPath = nullptr;
  const nfdchar_t *defaultPath =
      sLastDirectory.empty() ? nullptr : sLastDirectory.c_str();

  nfdresult_t result = NFD_OpenDialog(filter, defaultPath, &outPath);
  if (result == NFD_OKAY) {
    filePath = outPath;
    sLastDirectory = DirectoryOf(filePath);
    free(outPath);
  }

  return filePath;
}

std::string FileDialogWin::SaveFile(const char *filter) {
  std::string filePath;

  nfdchar_t *outPath = nullptr;
  const nfdchar_t *defaultPath =
      sLastDirectory.empty() ? nullptr : sLastDirectory.c_str();

  nfdresult_t result = NFD_SaveDialog(filter, defaultPath, &outPath);
  if (result == NFD_OKAY) {
    filePath = outPath;
    sLastDirectory = DirectoryOf(filePath);
    free(outPath);
  }

  return filePath;
}

std::string FileDialogWin::OpenImageFile(const char *filter) {
  return OpenFile(filter);
}
