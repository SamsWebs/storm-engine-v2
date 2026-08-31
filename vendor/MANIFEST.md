# Vendored dependencies

What is vendored here, where it came from, and what we have changed.

Everything in `vendor/` is frozen — checked into the repo or pinned as a
submodule — so nothing drifts on its own. What was missing until now was the
*record*. Without it an update is a blind swap: nobody can say what version is
in the tree, and a local fix applied to vendored source is invisible to whoever
next replaces that source, so it gets silently reverted.

**If you patch a vendored file, add it to "Local changes" below.** That is the
entire point of this file.

## Checked into the repository

Frozen by being committed. Headers only unless noted.

| Dependency | Version | Upstream | Files |
|---|---|---|---|
| Dear ImGui | **1.79 WIP** (`IMGUI_VERSION_NUM` 17803) | https://github.com/ocornut/imgui | `imgui.{h,cpp}`, `imgui_draw.cpp`, `imgui_widgets.cpp`, `imgui_demo.cpp`, `imgui_internal.h`, `imconfig.h`, `imgui_stdlib.{h,cpp}` |
| ImGuiFileDialog | **v0.5.1** | https://github.com/aiekick/ImGuiFileDialog | `ImGuiFileDialog.{h,cpp}`, `ImGuiFileDialogConfig.h` |
| imgui_sdl | unversioned upstream | https://github.com/Tyyppi77/imgui_sdl | `imgui_sdl.{h,cpp}` |
| ImGui SDL2 backend | ships with Dear ImGui | https://github.com/ocornut/imgui | `imgui_impl_sdl.{h,cpp}` |
| stb (rectpack, textedit, truetype) | ships with Dear ImGui | https://github.com/nothings/stb | `imstb_*.h` |
| sol2 | **3.2.3** (`SOL_VERSION_STRING`) | https://github.com/ThePhD/sol2 | `sol.hpp`, `config.hpp`, `forward.hpp` |
| Lua | **5.3.5** (`LUA_VERSION_*`) | https://www.lua.org/ | headers only: `lua.h`, `lua.hpp`, `luaconf.h`, `lauxlib.h`, `lualib.h` |
| FakeIt | unrecorded — single header, carries no version macro | https://github.com/eranpeer/FakeIt | `fakeit.h` |
| Native File Dialog | unrecorded — header only, no library shipped | https://github.com/mlabbe/nativefiledialog | `nfd.h` |

Two notes on the odd ones:

- **Lua is headers only and nothing links it.** `base.mk` records why `-llua`
  was dropped: nothing in `common/`, `specs/` or `editor/` references a `lua_`
  symbol, the editor parses `.lua` project files by hand, and Debian ships no
  versionless `liblua.so`. The headers are here because sol2 needs them to
  compile, not because Lua is used.

  The editor's Makefile adds `-I$(ROOT_DIR)/vendor/lua` for that same reason.
  sol2 includes `<lua.h>` unqualified, while the shared `INCLUDE` in `base.mk`
  only reaches `vendor/`, so `<lua/lua.h>` would be the only spelling that
  resolves. The flag is editor-only because no example includes sol2, and
  putting it in the shared `INCLUDE` would add a dependency to every example's
  compile line that none of them uses.
- **NFD ships no library.** Exactly one file uses it — the editor's
  `FileDialogWin.cpp` — and `base.mk` keeps `-lnfd` out of the shared `LIB` for
  that reason, so examples do not fail to link on machines without it.

## Pinned as submodules

Used by the Android build only. These are properly pinned already — git records
the exact commit — and are listed so the set is visible in one place.

| Submodule | Pinned commit | Upstream |
|---|---|---|
| `vendor/android/SDL2` | `fa24d868ac2f` | https://github.com/libsdl-org/SDL.git |
| `vendor/android/SDL_image2` | `c1bf2245b0ba` | https://github.com/libsdl-org/SDL_image.git |
| `vendor/android/SDL_mixer2` | `171eb2d420d5` | https://github.com/libsdl-org/SDL_mixer.git |
| `vendor/android/SDL_ttf2` | `4a318f8dfaa1` | https://github.com/libsdl-org/SDL_ttf.git |
| `vendor/android/glm` | `0af55ccecd98` | https://github.com/g-truc/glm.git |
| `vendor/android/tinyxml2` | `321ea883b719` | https://github.com/leethomason/tinyxml2.git |

Submodule pins move with `git submodule update --remote` and are recorded in the
commit that moves them, so they need no manual bookkeeping here.

## Local changes

Patches carried on top of upstream. **Re-apply or re-check each of these when
the dependency is updated.**

### `imgui/ImGuiFileDialog.cpp` — add `<cstdint>`

Upstream uses `intptr_t` at what is line 1187 in v0.5.1 without including a
header that declares it. It compiled on older toolchains because some other
header happened to pull it in transitively; GCC 13 tightened that and the file
stopped building:

```
ImGuiFileDialog.cpp:1187:49: error: 'intptr_t' was not declared in this scope
```

One line added to the include block. Upstream has since fixed this
independently, so updating ImGuiFileDialog past v0.5.1 should make the patch
unnecessary — check before re-applying it.

## Updating something here

1. Replace the files, or move the submodule pin.
2. Update its row above, including the version.
3. Check every entry under **Local changes** that touches it: is the patch still
   needed, or has upstream fixed it? Delete the entry if it is obsolete.
4. Build the editor as well as the engine. Most of `vendor/` exists for the
   editor, so an engine build passing says little about it.

## Worth deciding, not tracked here

Dear ImGui is at **1.79 WIP**, released around 2020. That may be entirely
deliberate — the editor works, and a frozen dependency is a legitimate choice —
or it may simply be that nobody could tell what version it was. Now that the
version is written down, that is a decision someone can make on evidence.
