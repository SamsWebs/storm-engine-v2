# CMake toolchain file for MinGW-w64 cross-compilation from Linux.
# Used by Makefile.win to build vendored SDL2 + satellites for Windows.
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# The -posix variants, matching Makefile.win's CXX. Debian ships mingw-w64 gcc
# twice, once per threading model, and the unsuffixed names are alternatives
# that can point at either. The two stages link into one binary — tinyxml2 is
# C++ and is built static here, then linked into engine objects compiled by
# Makefile.win — so a split threading model means two libstdc++ ABIs in one
# link. It does not fail loudly; it corrupts. Posix is the side that has to
# win because specs/net/netLoopback.spec.cpp uses <thread>, which the
# win32-threads gcc does not implement. If you change this, change
# Makefile.win and examples/examples.win.mk to match and rebuild deps from
# scratch (make -f Makefile.win distclean) — stale deps keep the old model.
set(CMAKE_C_COMPILER   x86_64-w64-mingw32-gcc-posix)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++-posix)
set(CMAKE_RC_COMPILER   x86_64-w64-mingw32-windres)

set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
