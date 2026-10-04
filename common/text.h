#pragma once

#include <algorithm>
#include <string>
#include <vector>

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

namespace storm {

// Drawing one line of text with SDL_ttf is a five-call dance - render to a
// surface, make a texture from it, query its size, copy it, free both - and
// every step has a failure path. Four examples wrote their own copy of it and
// the copies diverged: two were correct, one skipped the null-texture check,
// and one re-opened the font from disk on every call.
//
// Header-only and free of engine types, so it costs nothing to anyone who does
// not include it and reaches every platform target. The statics are scoped on
// a struct rather than being free functions because the engine has no
// namespace (KNOWN_ISSUES.md #9) and `DrawText` is a name games already use.
//
// Fonts come from AssetStore::GetFont. Nothing here opens or closes one.
// What FitText() and DrawFitted() report, and what FitFooter() lays out.
//
// These are at namespace scope rather than nested inside Text, because a
// caller names them in a signature and a return type buried inside a
// namespace-ish struct is a qualification nobody wants to type.
struct FittedText {
  SDL_Point size{0, 0};   // what was (or would be) drawn
  std::string text;       // the text as drawn, which may be shorter
  bool truncated = false; // whether anything was cut
};

struct FooterLayout {
  std::vector<std::string> lines;
  int width = 0;         // the WIDEST line, which is what you align against
  bool dropped = false;  // parts did not fit the line budget
  bool overflow = false; // a part is wider than maxWidth on its own
};

struct Text {
  // Pixel size the string would occupy. {0, 0} for a null font or a
  // measurement failure, which is also what an empty string gives.
  static SDL_Point Measure(TTF_Font *font, const std::string &text) {
    SDL_Point size{0, 0};
    if (!font || text.empty()) {
      return size;
    }
    if (TTF_SizeText(font, text.c_str(), &size.x, &size.y) != 0) {
      return SDL_Point{0, 0};
    }
    return size;
  }

  // Draws with the top-left corner at (x, y) and returns the size drawn.
  // {0, 0} means nothing was drawn - a null renderer or font, an empty
  // string, or an SDL failure. Never leaks the intermediate surface or
  // texture, including on the failure paths.
  static SDL_Point Draw(SDL_Renderer *renderer, TTF_Font *font,
                        const std::string &text, int x, int y,
                        SDL_Color color) {
    if (!renderer || !font || text.empty()) {
      return SDL_Point{0, 0};
    }

    SDL_Surface *surface = TTF_RenderText_Blended(font, text.c_str(), color);
    if (!surface) {
      return SDL_Point{0, 0};
    }

    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    const SDL_Point size{surface->w, surface->h};
    SDL_FreeSurface(surface);
    if (!texture) {
      return SDL_Point{0, 0};
    }

    const SDL_Rect destination{x, y, size.x, size.y};
    SDL_RenderCopy(renderer, texture, nullptr, &destination);
    SDL_DestroyTexture(texture);
    return size;
  }

  // Same, horizontally centred on centreX. Replaces the hand-guessed offsets
  // ("windowWidth / 2 - 30") that every example was carrying, which drift the
  // moment the string or the point size changes.
  static SDL_Point DrawCentred(SDL_Renderer *renderer, TTF_Font *font,
                               const std::string &text, int centreX, int y,
                               SDL_Color color) {
    const SDL_Point size = Measure(font, text);
    if (size.x == 0) {
      return size;
    }
    return Draw(renderer, font, text, centreX - size.x / 2, y, color);
  }

  // Same, with the text's RIGHT edge at rightX. The third alignment, so that
  // left/centre/right all exist and nobody computes the offset by hand: a
  // hand-computed right offset is off by the difference between the measured
  // width and the guess, which is invisible until the string changes.
  static SDL_Point DrawRight(SDL_Renderer *renderer, TTF_Font *font,
                             const std::string &text, int rightX, int y,
                             SDL_Color color) {
    const SDL_Point size = Measure(font, text);
    if (size.x == 0) {
      return size;
    }
    return Draw(renderer, font, text, rightX - size.x, y, color);
  }

  // ── A measured fit that MARKS truncation ───────────────────────────────────
  //
  // What happens today when a string is too long is that the caller draws it
  // anyway and lets it run off the edge, or clips it. Both leave the player
  // looking at something that looks deliberate. A silent truncation is a lie
  // about what the screen says, so a truncated string gets a mark and the
  // caller is told.
  // The default mark. A single ellipsis codepoint rather than "..." because it
  // is one glyph wide in most fonts and it cannot be mistaken for part of a
  // filename or a score.
  static const char *DefaultTruncationMark() { return "\xE2\x80\xA6"; }

  // Shortens `text` until it measures no wider than `maxWidth`, appending
  // `mark` when it had to cut, and reports what happened. Measures only --
  // DrawFitted() is the drawing half, and keeping them apart is what lets a
  // caller lay out before it commits pixels.
  //
  // The decisions, because each is one a plausible implementation gets wrong:
  //
  //  * The MARK IS INSIDE THE LIMIT. Truncating to the limit and then
  //    appending the mark overflows by however wide the mark is, which is the
  //    off-by-a-glyph this function exists to not have.
  //  * Cutting happens on a CHARACTER boundary. A byte-wise cut of a UTF-8
  //    string leaves a lone continuation byte, and TTF then renders a
  //    replacement box or nothing at all where the last character should be.
  //  * A part wider than the limit is cut as far as it can go, including the
  //    mark. If not even the mark fits, the result is EMPTY and truncated is
  //    true: returning a mark that overflows contradicts the limit, and
  //    returning the text unmarked claims it fit.
  //  * Text that already fits is returned byte-for-byte unchanged, with
  //    truncated false. A fit helper that "helpfully" normalised a passing
  //    string is a fit helper nobody trusts.
  static FittedText FitText(TTF_Font *font, const std::string &text,
                            int maxWidth,
                            const std::string &mark = DefaultTruncationMark()) {
    FittedText result;
    if (!font || text.empty()) {
      return result;
    }
    const SDL_Point full = Measure(font, text);
    if (maxWidth <= 0 || full.x <= maxWidth) {
      result.size = full;
      result.text = text;
      return result;
    }

    // Shrink from the whole string until the text AND the mark together fit.
    // One codepoint at a time, because a cut inside a multi-byte character is
    // the bug above.
    //
    // Starting at the FULL length and shrinking is load-bearing. An earlier
    // draft started `keep` at 0 and only ever decremented it, so it tested the
    // empty prefix, found the mark fit, and returned "…" for every width from
    // 40px to 120px -- a function that truncates a 180px string to a single
    // glyph and calls it a fit. The spec caught it only because one case
    // asserts the exact surviving text; the "never wider than the limit" case
    // passed happily, because "…" really is narrower than the limit. A
    // property-only spec would have shipped this.
    std::size_t keep = text.size();
    bool fits = false;
    while (true) {
      if (Measure(font, text.substr(0, keep) + mark).x <= maxWidth) {
        fits = true;
        break;
      }
      if (keep == 0) {
        break; // even the bare mark does not fit
      }
      // Step back to the start of the previous codepoint.
      std::size_t back = 1;
      while (back < keep &&
             (static_cast<unsigned char>(text[keep - back]) & 0xC0) == 0x80) {
        ++back;
      }
      keep -= back;
    }

    result.truncated = true;
    if (!fits) {
      // Not even the mark fits at this width. Empty, and still truncated, so
      // the caller knows something was there and could not be shown.
      return result;
    }
    result.text = text.substr(0, keep) + mark;
    result.size = Measure(font, result.text);
    return result;
  }

  // Draws the fitted text and returns what it did, so the drawn pixels and the
  // reported truncation cannot disagree.
  static FittedText
  DrawFitted(SDL_Renderer *renderer, TTF_Font *font, const std::string &text,
             int maxWidth, int x, int y, SDL_Color color,
             const std::string &mark = DefaultTruncationMark()) {
    const FittedText fitted = FitText(font, text, maxWidth, mark);
    if (fitted.text.empty()) {
      return fitted;
    }
    const SDL_Point drawn = Draw(renderer, font, fitted.text, x, y, color);
    if (drawn.x == 0) {
      // Nothing reached the screen (null renderer, SDL failure). Report the
      // size as zero rather than claiming a fit that did not happen.
      FittedText none = fitted;
      none.size = SDL_Point{0, 0};
      return none;
    }
    return fitted;
  }

  // ── A footer built from parts, so it can break BETWEEN them ────────────────
  //
  // A footer is several verbs, not one string. Joining them into a single
  // literal and hoping it fits is why footers get cut off at the edge, and a
  // part that gets cut is a control the screen named only halfway.
  // Breaks `parts` into lines that each measure no wider than `maxWidth`,
  // breaking ONLY between parts. Separator sits BETWEEN parts, never at the
  // end of a line.
  static FooterLayout FitFooter(TTF_Font *font,
                                const std::vector<std::string> &parts,
                                int maxWidth, int maxLines,
                                const std::string &separator = "  ") {
    FooterLayout layout;
    if (!font || parts.empty() || maxLines <= 0) {
      return layout;
    }

    std::string line;
    for (const std::string &part : parts) {
      if (part.empty()) {
        continue;
      }
      const std::string candidate =
          line.empty() ? part : line + separator + part;
      if (Measure(font, candidate).x <= maxWidth || line.empty()) {
        // `|| line.empty()` is the decision that matters: a part too wide for
        // the limit gets a line to itself, WHOLE, and is flagged below. It is
        // never cut -- a control name sliced in half is worse than one that
        // overhangs, and cutting it here would be the same silent truncation
        // FitText exists to make honest.
        line = candidate;
        if (Measure(font, line).x > maxWidth) {
          layout.overflow = true;
        }
        continue;
      }
      layout.lines.push_back(line);
      line = part;
      if (Measure(font, line).x > maxWidth) {
        layout.overflow = true;
      }
      if (static_cast<int>(layout.lines.size()) >= maxLines) {
        // Budget spent. The part in hand and everything after it are dropped,
        // and `dropped` says so -- a footer that quietly shows three of five
        // controls looks like a complete list of three.
        layout.dropped = true;
        break;
      }
    }
    if (layout.dropped) {
      // A dropped trailing line is not part of the layout.
      line.clear();
    } else if (!line.empty()) {
      layout.lines.push_back(line);
    }

    for (const std::string &l : layout.lines) {
      layout.width = std::max(layout.width, Measure(font, l).x);
    }
    return layout;
  }

  // Draws the laid-out footer, one line per layout line, `lineHeight` pixels
  // apart, and returns how many lines it drew. `camera` is accepted so a
  // scrolled world's footer can pan with it; null means screen space.
  static int DrawFooter(SDL_Renderer *renderer, TTF_Font *font,
                        const std::vector<std::string> &parts, int maxWidth,
                        int x, int y, int lineHeight, SDL_Color color,
                        const SDL_Rect *camera = nullptr,
                        const std::string &separator = "  ") {
    if (!renderer || !font) {
      return 0;
    }
    const FooterLayout layout = FitFooter(font, parts, maxWidth, 8, separator);
    const int step = lineHeight > 0 ? lineHeight : Measure(font, "Ag").y;
    int drawn = 0;
    for (std::size_t i = 0; i < layout.lines.size(); ++i) {
      const int lineX = camera != nullptr ? x - camera->x : x;
      const int lineY = camera != nullptr ? y - camera->y : y;
      const SDL_Point size = Draw(renderer, font, layout.lines[i], lineX,
                                  lineY + static_cast<int>(i) * step, color);
      if (size.x > 0) {
        ++drawn;
      }
    }
    return drawn;
  }
};

} // namespace storm
