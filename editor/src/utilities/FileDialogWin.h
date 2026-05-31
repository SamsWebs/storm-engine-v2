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
  /*
   *  	OpenFile --> Opens a windows file dialog. By default, the filters are
   *set to .lua and .map files; however, we only truly need to open the project
   *file, which is a lua file. The project loader will load any map files for
   *us. The function returns the path as an std::string of the file to open if
   *successful, if not returns an empty string.
   */

  // NFD filter format: comma-separated extensions, e.g. "lua,map"
  static std::string OpenFile(const char *filter = "lua");

  /*
   *  	SaveFile --> Opens a native file dialog.
   *	The function returns the path as an std::string of the file to save if
   *successful, if not returns an empty string.
   */
  static std::string SaveFile(const char *filter = "lua");

  /*
   *  	OpenImageFile --> Opens a native file dialog.
   *	The function returns the path as an std::string of the image we want to
   *use if successful, if not returns an empty string.
   */
  static std::string OpenImageFile(const char *filter = "png,bmp,jpg,jpeg");
};