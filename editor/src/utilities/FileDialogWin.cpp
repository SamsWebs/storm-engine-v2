#include "FileDialogWin.h"

std::string FileDialogWin::OpenFile(const char *filter, Window owner) {
  std::string filePath;

  // Open display connection
  Display *display = XOpenDisplay(NULL);
  if (display == NULL) {
    // Handle display opening error
    return filePath;
  }

  // Create a window to act as the dialog's owner
  Window rootWindow = DefaultRootWindow(display);
  Window dialogOwner =
      XCreateSimpleWindow(display, rootWindow, 0, 0, 1, 1, 0, 0, 0);

  // Map the temporary window to the screen
  XMapWindow(display, dialogOwner);
  XFlush(display);

  // Create a file dialog window using Xlib
  Window fileDialog =
      XCreateSimpleWindow(display, dialogOwner, 10, 10, 400, 300, 2, 0, 0);
  XSelectInput(display, fileDialog, ButtonPressMask);
  XMapWindow(display, fileDialog);

  // Event loop for the file dialog
  XEvent event;
  Atom wmDeleteWindow = XInternAtom(display, "WM_DELETE_WINDOW", False);
  XSetWMProtocols(display, fileDialog, &wmDeleteWindow, 1);

  bool running = true;
  while (running) {
    XNextEvent(display, &event);
    switch (event.type) {
    case ButtonPress:
      // Here, you should handle the file selection logic based on mouse click
      // For simplicity, let's assume that you close the dialog with no
      // selection
      running = false;
      break;
    case ClientMessage:
      if (event.xclient.message_type ==
              XInternAtom(display, "WM_PROTOCOLS", True) &&
          Atom(event.xclient.data.l[0]) ==
              XInternAtom(display, "WM_DELETE_WINDOW", True)) {
        // Handle the window close event (e.g., user clicked the close button)
        running = false;
      }
      break;
    }
  }

  // Destroy the file dialog window and the temporary owner window
  XDestroyWindow(display, fileDialog);
  XDestroyWindow(display, dialogOwner);
  XFlush(display);

  // Close display connection
  XCloseDisplay(display);

  return filePath;
}

// std::string FileDialogWin::OpenFile(const char *filter, Window owner) {
//   std::string filePath;

//   // Open display connection
//   Display *display = XOpenDisplay(NULL);
//   if (display == NULL) {
//     // Handle display opening error
//     return filePath;
//   }

//   // Create a window to act as the dialog's owner
//   Window rootWindow = DefaultRootWindow(display);
//   Window dialogOwner =
//       XCreateSimpleWindow(display, rootWindow, 0, 0, 1, 1, 0, 0, 0);

//   // Show the dialog
//   XEvent event;
//   XEvent dummyEvent;
//   std::fill(reinterpret_cast<char *>(&event),
//             reinterpret_cast<char *>(&event) + sizeof(event), 0);
//   std::fill(reinterpret_cast<char *>(&dummyEvent),
//             reinterpret_cast<char *>(&dummyEvent) + sizeof(dummyEvent), 0);
//   Atom wmDeleteWindow = XInternAtom(display, "WM_DELETE_WINDOW", False);
//   XSetWMProtocols(display, dialogOwner, &wmDeleteWindow, 1);

//   XSetIOErrorHandler([](Display *) { return 0; });

//   XFlush(display);
//   XSync(display, False);

//   XUnmapWindow(display, dialogOwner);
//   XDestroyWindow(display, dialogOwner);
//   XFlush(display);
//   XSync(display, False);

//   // Get the selected file path
//   if (!filePath.empty()) {
//     // Perform any necessary processing on the file path
//   }

//   // Destroy the temporary window
//   XDestroyWindow(display, dialogOwner);

//   // Close display connection
//   XCloseDisplay(display);

//   return filePath;
// }

std::string FileDialogWin::SaveFile(const char *filter, Window owner) {
  std::string filePath;

  // Open display connection
  Display *display = XOpenDisplay(NULL);
  if (display == NULL) {
    // Handle display opening error
    return filePath;
  }

  // Create a window to act as the dialog's owner
  Window dialogOwner = XCreateSimpleWindow(display, owner, 0, 0, 1, 1, 0, 0, 0);

  // Show the dialog
  // Use Xlib functions to display the save file dialog and retrieve the
  // selected file path Here, you will need to implement the Xlib-based file
  // dialog functionality for saving files

  if (!filePath.empty()) {
    // Perform any necessary processing on the file path
  }

  // Close display connection
  XCloseDisplay(display);

  return filePath;
}

std::string FileDialogWin::OpenImageFile(const char *filter, Window owner) {
  return OpenFile(filter, owner);
}
