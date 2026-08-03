cmake_minimum_required(VERSION 3.22)
project(storm_platformer)

# Repo root (app/jni -> app -> android-platformer -> examples -> root)
get_filename_component(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../.." ABSOLUTE)
set(EXAMPLE_DIR "${REPO_ROOT}/examples/android-platformer")

# ── Vendored dependencies (pinned submodules under vendor/android) ──────────
# SDL2 and SDL2_image MUST be shared: SDLActivity loads libSDL2.so /
# libSDL2_image.so by name at startup (getLibraries in PlatformerActivity).
set(SDL_SHARED ON  CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST   OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_SAMPLES OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_VENDORED OFF CACHE BOOL "" FORCE)  # stb backend: png/jpg, no libpng needed
set(SDL2TTF_SAMPLES OFF CACHE BOOL "" FORCE)
set(SDL2TTF_VENDORED ON CACHE BOOL "" FORCE)     # builds FreeType from external/freetype
set(SDL2TTF_HARFBUZZ OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_SAMPLES OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_VENDORED OFF CACHE BOOL "" FORCE)  # wav built-in, mp3 via minimp3, ogg via stb_vorbis
set(SDL2MIXER_FLAC OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_MOD OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_MIDI OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_OPUS OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_WAVPACK OFF CACHE BOOL "" FORCE)   # no system libwavpack in the NDK
set(tinyxml2_BUILD_TESTING OFF CACHE BOOL "" FORCE)

set(BUILD_SHARED_LIBS ON)                        # satellite libs -> libSDL2_*.so
add_subdirectory("${REPO_ROOT}/vendor/android/SDL2"       SDL)
add_subdirectory("${REPO_ROOT}/vendor/android/SDL_image2" SDL_image)
add_subdirectory("${REPO_ROOT}/vendor/android/SDL_ttf2"   SDL_ttf)
add_subdirectory("${REPO_ROOT}/vendor/android/SDL_mixer2" SDL_mixer)

set(BUILD_SHARED_LIBS OFF)                       # tinyxml2 static, into libmain.so
add_subdirectory("${REPO_ROOT}/vendor/android/tinyxml2"  tinyxml2)

# SDL_image's source tree ships include/SDL_image.h — the SDL2/ prefix used by
# the engine's #include <SDL2/SDL_image.h> is added by distro packaging at
# install time (SDL2 itself generates its SDL2/ dir in the build tree; SDL_image
# does not). Stage a compat copy so the desktop include path works unchanged.
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/compat/SDL2")
file(COPY "${REPO_ROOT}/vendor/android/SDL_image2/include/SDL_image.h"
     DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/compat/SDL2")
file(COPY "${REPO_ROOT}/vendor/android/SDL_ttf2/SDL_ttf.h"
     DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/compat/SDL2")
file(COPY "${REPO_ROOT}/vendor/android/SDL_mixer2/include/SDL_mixer.h"
     DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/compat/SDL2")

# ── Engine core + game sources into the single JNI library ──────────────────
# Recursive: a plain common/*.cpp glob picks up 6 of the 13 translation units
# and silently drops all of common/net/, so anything touching Net* fails to
# link. Networking also needs the INTERNET permission in AndroidManifest.xml.
file(GLOB_RECURSE ENGINE_SRC "${REPO_ROOT}/common/*.cpp")
file(GLOB GAME_SRC   "${EXAMPLE_DIR}/src/*.cpp" "${EXAMPLE_DIR}/src/states/*.cpp")

add_library(main SHARED ${ENGINE_SRC} ${GAME_SRC})

target_include_directories(main PRIVATE
    "${EXAMPLE_DIR}/include"                     # <stormengine2/...> -> common/
    "${REPO_ROOT}/vendor/android/glm"            # header-only glm
    "${REPO_ROOT}/examples/platformer/src"       # shared playerComponent.h
    "${REPO_ROOT}/vendor/android/SDL2/include"
    "${REPO_ROOT}/vendor/android/SDL_image2/include"
    "${CMAKE_CURRENT_BINARY_DIR}/compat"         # SDL2/SDL_image.h shim
)

target_link_libraries(main
    SDL2::SDL2main
    SDL2::SDL2
    SDL2_image
    SDL2_ttf
    SDL2_mixer
    tinyxml2::tinyxml2
    log android
)
