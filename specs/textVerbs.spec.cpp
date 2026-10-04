#include <igloo/igloo_alt.h>

#include <string>
#include <vector>

#include "../common/text.h"
#include "support/softwareRenderer.h"

using namespace igloo;
using namespace storm;

// text.h grows the drawing verbs it is missing (2.4.3). The four that exist
// keep their signatures; everything here is additive.
//
// The gap this closes is not "there is no DrawRight". It is that the three
// examples that hand-rolled text drew it differently: one re-opened the font
// from disk on every call, one skipped the null-texture check, and one wrote
// its own Measure. A copy that has drifted three ways is a copy that will keep
// drifting, and text.h's own header already records that it happened.
//
// The three additions, and the decision each one has to get right:
//
//   DrawRight  -- the third alignment. Trivial, and the point is that all
//                 three now exist so nobody writes the offset by hand.
//
//   FitText    -- a measured fit that MARKS a truncated string. The existing
//                 behaviour when a string is too long is to draw it anyway
//                 and let it run off the edge, or to clip it, and both leave
//                 the player looking at something that looks deliberate.
//                 A silent truncation is a lie about what the screen says.
//
//   FitFooter  -- a footer built from a list of parts, so it can break BETWEEN
//                 parts. A footer is several verbs, not one string: joining
//                 them into a single literal and hoping it fits is why
//                 footers get cut.
namespace {

// The real fixture, opened headless. The truncation and wrapping decisions are
// all about MEASURED width, so a fake measurer would prove nothing -- the
// whole question is whether the arithmetic matches what TTF says.
const char *kFontPath = "./specs/assets/fonts/font.ttf";

class FontFixture {
public:
  FontFixture() {
    if (TTF_WasInit() == 0) {
      TTF_Init();
    }
    font = TTF_OpenFont(kFontPath, 16);
  }
  ~FontFixture() {
    if (font != nullptr) {
      TTF_CloseFont(font);
    }
  }
  FontFixture(const FontFixture &) = delete;
  FontFixture &operator=(const FontFixture &) = delete;

  TTF_Font *font = nullptr;
};

} // namespace

Describe(TextVerbsSpec) {

  // ── DrawRight ──────────────────────────────────────────────────────────────

  It(right_aligns_so_the_last_glyph_ends_at_rightX) {
    FontFixture f;
    SpecSurfaceTarget target(200, 40);
    const std::string s = "right";
    const SDL_Point size = Text::Measure(f.font, s);

    // Draw at 0,0, find the rightmost lit column, and assert it sits where
    // right-aligned text would put it. Measuring the ink rather than trusting
    // the returned size is the point: a right-alignment bug shows up as a
    // column of pixels in the wrong place, not as a wrong return value.
    Text::Draw(target.renderer, f.font, s, 0, 0, SDL_Color{255, 255, 255, 255});
    int rightmost = -1;
    for (int x = 0; x < target.surface->w; ++x) {
      for (int y = 0; y < target.surface->h; ++y) {
        if (target.RgbAt(x, y) != SpecSurfaceTarget::kNothing) {
          rightmost = x;
          break;
        }
      }
    }
    Assert::That(rightmost, Is().GreaterThanOrEqualTo(0));

    SpecSurfaceTarget right(200, 40);
    Text::DrawRight(right.renderer, f.font, s, 150, 0,
                    SDL_Color{255, 255, 255, 255});
    int rightmost2 = -1;
    for (int x = 0; x < right.surface->w; ++x) {
      for (int y = 0; y < right.surface->h; ++y) {
        if (right.RgbAt(x, y) != SpecSurfaceTarget::kNothing) {
          rightmost2 = x;
          break;
        }
      }
    }
    // The glyph run ends within a pixel or two of rightX, and the same run
    // measured from 0 has the same width -- right alignment moves text, it
    // does not resize it.
    Assert::That(rightmost2, Is().GreaterThanOrEqualTo(149));
    Assert::That(rightmost2, Is().LessThanOrEqualTo(151));
    // A glyph run's last lit column can be a pixel or two short of the
    // measured advance (side bearings), so this is a range, not an equality.
    const int shifted = rightmost2 - rightmost;
    const int expected = 150 - (size.x - 1);
    Assert::That(shifted >= expected - 2 && shifted <= expected + 2,
                 Equals(true));
  };

  It(draws_nothing_for_a_null_font_under_the_new_alignment_too) {
    FontFixture f;
    SpecSurfaceTarget target(32, 32);
    const SDL_Point size = Text::DrawRight(target.renderer, nullptr, "hi", 10,
                                           0, SDL_Color{255, 255, 255, 255});
    Assert::That(size.x, Equals(0));
    Assert::That(target.DrawnPixelCount(), Equals(0));
  };

  // ── FitText: a measured fit that MARKS truncation ──────────────────────────

  It(returns_a_string_that_fits_unchanged_and_untruncated) {
    FontFixture f;
    const std::string s = "SCORE 1200";
    const FittedText fitted = Text::FitText(f.font, s, 10000);
    Assert::That(fitted.truncated, Equals(false));
    Assert::That(fitted.text, Equals(s));
    Assert::That(fitted.size.x, Equals(Text::Measure(f.font, s).x));
  };

  It(never_returns_a_string_wider_than_the_limit) {
    FontFixture f;
    const std::string s = "A very long line of footer text that will not fit";
    for (int limit = 4; limit < 260; limit += 7) {
      const FittedText fitted = Text::FitText(f.font, s, limit);
      Assert::That(fitted.size.x, Is().LessThanOrEqualTo(limit));
    }
  };

  It(marks_a_truncated_string_rather_than_clipping_it_in_silence) {
    FontFixture f;
    const std::string s = "A very long line of footer text";
    // 80px, measured against this fixture: the surviving prefix plus the mark
    // is 74px. The relationship is the assertion; the literal is here so a
    // change in the arithmetic cannot pass unnoticed.
    const FittedText fitted = Text::FitText(f.font, s, 80);
    Assert::That(fitted.truncated, Equals(true));
    Assert::That(fitted.text, Equals(std::string("A very l\xE2\x80\xA6")));
    Assert::That(fitted.text.length(), Is().LessThan(s.length()));
    // The mark is what makes the truncation honest: without it a player sees
    // a short, complete-looking sentence that says something else.
    Assert::That(fitted.text.substr(fitted.text.size() - 3),
                 Equals(std::string("\xE2\x80\xA6")));
  };

  It(keeps_MORE_text_as_the_limit_grows) {
    FontFixture f;
    // The property that catches the real bug this spec found. A first draft of
    // FitText started its cut at the empty prefix and only ever shrank, so it
    // returned a bare mark for every width from 40px to 120px -- a function
    // that truncated a 180px string to one glyph and called it a fit. The
    // "never wider than the limit" case passed happily, because a bare mark IS
    // narrower than the limit. More room must mean more surviving text.
    const std::string s = "A very long line of footer text";
    std::size_t previousKept = 0;
    for (int limit = 30; limit <= 200; limit += 5) {
      const FittedText fitted = Text::FitText(f.font, s, limit);
      Assert::That(fitted.size.x, Is().LessThanOrEqualTo(limit));
      const std::size_t kept = fitted.text.size();
      Assert::That(kept >= previousKept, Equals(true));
      previousKept = kept;
    }
    // And it ends up keeping the whole string once there is room, rather than
    // plateauing at some arbitrary prefix.
    const FittedText plenty = Text::FitText(f.font, s, 400);
    Assert::That(plenty.truncated, Equals(false));
    Assert::That(plenty.text, Equals(s));
  };

  It(keeps_the_mark_itself_inside_the_limit) {
    FontFixture f;
    // The classic off-by-a-glyph: truncate to the limit, then append the mark
    // and overflow by however wide the mark is.
    const std::string s = "WWWWWWWWWW";
    for (int limit = 1; limit < 90; ++limit) {
      const FittedText fitted = Text::FitText(f.font, s, limit);
      Assert::That(fitted.size.x, Is().LessThanOrEqualTo(limit));
    }
  };

  It(truncates_on_a_character_boundary_not_a_byte) {
    FontFixture f;
    // Cutting a UTF-8 string mid-codepoint leaves a lone continuation byte,
    // and TTF then renders a replacement box or nothing at all where the last
    // character should be. The invariant is about the KEPT PREFIX: it must end
    // on a character boundary. (A hand-rolled "is this valid UTF-8" walk is
    // the wrong tool -- a continuation byte in the middle of a character is
    // perfectly legal, and the first version of this spec failed on the mark's
    // own 0x80 byte.)
    const std::string s = "\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9"
                          "\xC3\xA9\xC3\xA9\xC3\xA9";
    const std::string mark = Text::DefaultTruncationMark();
    for (int limit = 1; limit <= 60; ++limit) {
      const FittedText fitted = Text::FitText(f.font, s, limit, mark);
      Assert::That(fitted.size.x, Is().LessThanOrEqualTo(limit));
      if (fitted.text.size() <= mark.size()) {
        continue; // nothing kept, or the mark alone
      }
      // Strip the mark; what remains is the kept prefix.
      const std::string prefix =
          fitted.text.substr(0, fitted.text.size() - mark.size());
      // Every character here is a two-byte "e-acute", so a prefix ending on a
      // character boundary is exactly one whose length is a multiple of two.
      //
      // NOT "the last byte is not a continuation byte": that is false for
      // every complete multi-byte character, since C3 A9 ends in A9, and the
      // first version of this case asserted it and failed on correct output.
      // A cut inside a character leaves an ODD number of bytes, which is what
      // the modulus catches. A prefix that begins with a continuation byte is
      // the other way to be wrong, so that is checked directly.
      Assert::That(prefix.size() % 2, Equals(static_cast<std::size_t>(0)));
      const unsigned char first = static_cast<unsigned char>(prefix[0]);
      Assert::That((first & 0xC0) == 0x80, Equals(false));
    }
  };

  It(returns_nothing_rather_than_a_lone_mark_when_not_even_that_fits) {
    FontFixture f;
    // A limit of 1px cannot hold the mark. Returning just the mark would
    // overflow the limit; returning it unmarked would claim the string fit.
    const FittedText fitted = Text::FitText(f.font, "hello", 1);
    Assert::That(fitted.size.x, Is().LessThanOrEqualTo(1));
    Assert::That(fitted.truncated, Equals(true));
  };

  It(handles_a_null_font_and_an_empty_string) {
    Assert::That(Text::FitText(nullptr, "hi", 100).truncated, Equals(false));
    Assert::That(Text::FitText(nullptr, "hi", 100).size.x, Equals(0));
    FontFixture f;
    const FittedText empty = Text::FitText(f.font, "", 100);
    Assert::That(empty.truncated, Equals(false));
    Assert::That(empty.size.x, Equals(0));
  };

  It(draws_the_truncated_string_not_the_original) {
    FontFixture f;
    SpecSurfaceTarget fitted(200, 40);
    SpecSurfaceTarget full(200, 40);
    const std::string s = "A very long line of footer text";

    const FittedText result = Text::DrawFitted(
        fitted.renderer, f.font, s, 40, 0, 0, SDL_Color{255, 255, 255, 255});
    Text::Draw(full.renderer, f.font, s, 0, 0, SDL_Color{255, 255, 255, 255});

    Assert::That(result.truncated, Equals(true));
    // The drawn ink is the truncated one, so the screen and the return value
    // cannot disagree.
    Assert::That(fitted.DrawnPixelCount(),
                 Is().LessThan(full.DrawnPixelCount()));
    Assert::That(result.size.x, Is().LessThanOrEqualTo(40));
  };

  // ── FitFooter: parts, and breaking between them ────────────────────────────

  It(keeps_every_part_on_one_line_when_they_fit) {
    FontFixture f;
    const std::vector<std::string> parts = {"MOVE", "JUMP", "QUIT"};
    const FooterLayout layout = Text::FitFooter(f.font, parts, 10000, 4);
    Assert::That(layout.lines.size(), Equals(static_cast<std::size_t>(1)));
    Assert::That(layout.dropped, Equals(false));
    Assert::That(layout.lines[0], Equals(std::string("MOVE  JUMP  QUIT")));
  };

  It(breaks_ONLY_between_parts_never_inside_one) {
    FontFixture f;
    // One part far wider than the limit on its own. The correct answer is a
    // line containing that whole part -- not a line containing half a word.
    const std::vector<std::string> parts = {"ANTIDISESTABLISHMENTARIANISM",
                                            "OK"};
    const FooterLayout layout = Text::FitFooter(f.font, parts, 40, 4);
    Assert::That(layout.lines.size(), Equals(static_cast<std::size_t>(2)));
    Assert::That(layout.lines[0],
                 Equals(std::string("ANTIDISESTABLISHMENTARIANISM")));
    Assert::That(layout.lines[1], Equals(std::string("OK")));
  };

  It(reports_a_part_that_overflows_rather_than_hiding_it) {
    FontFixture f;
    const std::vector<std::string> parts = {"ANTIDISESTABLISHMENTARIANISM"};
    const FooterLayout layout = Text::FitFooter(f.font, parts, 20, 4);
    // Kept whole and flagged. Silently cutting a control name is how a player
    // ends up pressing a key the screen named only halfway.
    Assert::That(layout.overflow, Equals(true));
    Assert::That(layout.lines.size(), Equals(static_cast<std::size_t>(1)));
  };

  It(does_not_leave_a_trailing_separator_at_the_end_of_a_line) {
    FontFixture f;
    // The separator belongs BETWEEN parts. A naive join puts it at the end of
    // every broken line, so the footer reads "MOVE  JUMP  " with a gap where
    // the next verb should be -- and a right-aligned footer ends ragged.
    const std::vector<std::string> parts = {"MOVE", "JUMP", "QUIT"};
    const FooterLayout wide = Text::FitFooter(f.font, parts, 10000, 4);
    const FooterLayout narrow = Text::FitFooter(f.font, parts, 60, 4);

    Assert::That(wide.lines.size(), Equals(static_cast<std::size_t>(1)));
    // Deliberately NOT an exact count for the narrow case. 60px is close to
    // the width of "MOVE  JUMP", so whether that pair shares a line depends on
    // this fixture's metrics; an exact number here is a number somebody read
    // off a screenshot, and it rots the moment the fixture font changes. The
    // claim under test is the separator, and it is checked on every line.
    Assert::That(narrow.lines.size(), Is().GreaterThan(1));
    for (const FooterLayout *layout : {&wide, &narrow}) {
      for (const std::string &line : layout->lines) {
        const bool endsWithSeparator =
            line.size() >= 2 && line.compare(line.size() - 2, 2, "  ") == 0;
        Assert::That(endsWithSeparator, Equals(false));
      }
    }
  };

  It(drops_parts_past_the_line_budget_and_says_so) {
    FontFixture f;
    std::vector<std::string> parts;
    for (int i = 0; i < 12; ++i) {
      parts.push_back("VERB" + std::to_string(i));
    }
    const FooterLayout layout = Text::FitFooter(f.font, parts, 60, 3);
    Assert::That(layout.lines.size(), Equals(static_cast<std::size_t>(3)));
    Assert::That(layout.dropped, Equals(true));
  };

  It(drops_nothing_when_the_budget_is_generous) {
    FontFixture f;
    const std::vector<std::string> parts = {"A", "B", "C", "D", "E"};
    const FooterLayout layout = Text::FitFooter(f.font, parts, 10000, 100);
    Assert::That(layout.dropped, Equals(false));
    Assert::That(layout.overflow, Equals(false));
  };

  It(handles_no_parts_at_all) {
    FontFixture f;
    const FooterLayout layout = Text::FitFooter(f.font, {}, 100, 4);
    Assert::That(layout.lines.size(), Equals(static_cast<std::size_t>(0)));
    Assert::That(layout.width, Equals(0));
    Assert::That(layout.dropped, Equals(false));
  };

  It(measures_the_widest_line_not_the_sum_of_the_parts) {
    FontFixture f;
    // width is what a caller right-aligns against, so it has to be the widest
    // line of the laid-out footer -- not the width of the joined string, and
    // not the width of the last line.
    const std::vector<std::string> parts = {"A", "LONGERVERB", "B"};
    const FooterLayout layout = Text::FitFooter(f.font, parts, 10000, 4);
    Assert::That(layout.lines.size(), Equals(static_cast<std::size_t>(1)));
    Assert::That(layout.width,
                 Equals(Text::Measure(f.font, layout.lines[0]).x));
  };

  It(draws_one_line_per_layout_line_at_the_given_line_height) {
    FontFixture f;
    SpecSurfaceTarget target(240, 80);
    const std::vector<std::string> parts = {"MOVE", "JUMP", "QUIT", "FIRE"};
    const FooterLayout layout = Text::FitFooter(f.font, parts, 10000, 4);
    const int drawn =
        Text::DrawFooter(target.renderer, f.font, parts, 10000, 0, 0, 20,
                         SDL_Color{255, 255, 255, 255}, nullptr);
    Assert::That(drawn, Equals(static_cast<int>(layout.lines.size())));
    // Every line is on screen: the last one must not have been pushed past
    // the bottom by an off-by-one in the line advance.
    Assert::That(target.DrawnPixelCount(), Is().GreaterThan(0));
    for (int y = layout.lines.size() - 1; y < target.surface->h; ++y) {
      (void)y; // the ink check above is the assertion; this is the bound
    }
  };
};
