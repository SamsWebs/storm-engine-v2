#pragma once
#include <string>

#include <imgui/imgui.h>
#include <nfd/nfd.h>

/*
 *	Windows platform implementation of the FileDialogs needed for loading
 *and saving files on a windows machine
 */
class FileDialogWin {
public:
  // NFD filter format: comma-separated extensions, e.g. "lua,map"
  static std::string OpenFile(const char *filter = "lua");
  static std::string SaveFile(const char *filter = "lua");
  static std::string OpenImageFile(const char *filter = "png,bmp,jpg,jpeg");

private:
  // Persists across calls so the dialog reopens in the last used directory
  static std::string sLastDirectory;

  // Extract the directory portion of a full file path
  static std::string DirectoryOf(const std::string &path);
};
