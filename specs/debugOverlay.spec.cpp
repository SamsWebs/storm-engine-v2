// DebugStats / DebugOverlay — 2.4.5.
//
// "The cheapest item on this list and still the right one", and the reason it
// is right is that 2.0.0's diagnostics currently only reach a log nobody reads
// DURING PLAY. A game's only diagnostic surface a player can see is the one it
// builds itself, and the thing a developer needs at 11pm is a frame time and
// the name of the slow system.
//
// The split here is the one that makes it testable, and it is the same split
// the rest of the engine uses: the NUMBERS are pure and take their clock from
// the caller, and drawing is a separate half. DebugStats never calls
// SDL_GetTicks. If it did, every case below would be a timing assertion, and a
// timing assertion is a flaky assertion that gets deleted.
#include <igloo/igloo_alt.h>

#include <string>
#include <vector>

#include "../common/debugOverlay.h"

using namespace igloo;
using namespace storm;

namespace {

// A deterministic stand-in for the log. DebugStats reads the recent errors out
// of a list it is handed, rather than reaching into Logger::messages itself,
// so this spec does not depend on a process-wide static and two cases cannot
// see each other's output.
std::vector<LogEntry> RecentErrors() {
  return {LogEntry{LOG_ERROR, "AssetStore: failed to open font 'ui.ttf'"},
          LogEntry{LOG_ERROR, "TileMapLoader: row 2 column 4 is not a number"},
          LogEntry{LOG_ERROR, "NetSocket: send returned -1"}};
}

// Whether any formatted line contains `word`. The overlay's output is TEXT, so
// asserting on the whole string would pin the exact layout -- and a layout
// nobody should depend on is exactly what changes when someone adds a column.
// The claim under test is "this name appears on screen", not "in this column
// with this padding".
bool ContainsWord(const std::vector<std::string> &lines,
                  const std::string &word) {
  for (const std::string &line : lines) {
    if (line.find(word) != std::string::npos) {
      return true;
    }
  }
  return false;
}

} // namespace

Describe(DebugStatsSpec) {

  It(reports_nothing_before_it_has_been_fed_a_frame) {
    // A freshly constructed overlay is on screen the moment a game toggles it,
    // often on frame one. Dividing by a zero elapsed time there is how an
    // overlay prints "inf fps" exactly when a developer most needs it to be
    // readable.
    DebugStats stats;
    const std::vector<std::string> lines = stats.FormatLines(RecentErrors(), 3);
    Assert::That(lines.empty(), Equals(true));
  };

  It(averages_fps_over_a_window_rather_than_reporting_the_instant) {
    DebugStats stats;
    // Instantaneous FPS is unreadable: one 4ms frame reports 250 and the next
    // 16ms frame reports 62, and the number flickers faster than it can be
    // read. A window is the whole reason the overlay is usable.
    stats.BeginWindow(64);
    for (int i = 0; i < 32; ++i) {
      stats.Frame(0.016); // ~60fps
    }
    const double fps = stats.AverageFps();
    // Half the window: an average over what has been fed, not a divide by the
    // full window size, which would report 30 for what is really 60.
    Assert::That(fps > 55.0 && fps < 65.0, Equals(true));
  };

  It(reports_zero_fps_rather_than_infinity_for_a_window_with_no_time) {
    DebugStats stats;
    stats.BeginWindow(64);
    stats.Frame(0.0);
    stats.Frame(0.0);
    // A paused or a first-frame sample. "inf" on a HUD is worse than "0",
    // because 0 is a number a reader can reason about.
    Assert::That(stats.AverageFps(), Equals(0.0));
  };

  It(forgets_samples_older_than_the_window) {
    DebugStats stats;
    stats.BeginWindow(4);
    for (int i = 0; i < 100; ++i) {
      stats.Frame(0.016);
    }
    stats.Frame(1.0); // one very long frame
    // A single hitch must not poison the average forever: the window is a
    // ring, so the long frame ages out and the reported rate recovers. An
    // overlay whose FPS never comes back is an overlay people turn off.
    for (int i = 0; i < 4; ++i) {
      stats.Frame(0.016);
    }
    const double fps = stats.AverageFps();
    Assert::That(fps > 55.0 && fps < 65.0, Equals(true));
  };

  It(names_the_slowest_system_and_totals_the_rest) {
    // "Which system is slow" is the question per-system timings exist to
    // answer, so the slowest is named rather than a table a reader has to
    // scan. The total is there because a frame can be slow with no single
    // system to blame.
    DebugStats stats;
    stats.BeginWindow(64);
    stats.BeginFrame(0.016);
    stats.BeginSystem("movement");
    stats.EndSystem(0.001);
    stats.BeginSystem("contact");
    stats.EndSystem(0.009);
    stats.BeginSystem("animation");
    stats.EndSystem(0.002);
    stats.EndFrame();
    const std::vector<std::string> lines = stats.FormatLines({}, 0);
    // The SLOWEST is named. A table of every system is a table a reader has to
    // scan, and "which system is slow" is the question this exists to answer.
    // (An earlier version of this case asserted that `movement` appeared too,
    // which contradicted the design rather than the code.)
    Assert::That(ContainsWord(lines, "contact"), Equals(true));
    Assert::That(ContainsWord(lines, "systems"), Equals(true));
  };

  It(ignores_a_system_that_never_ended_rather_than_reporting_a_wild_number) {
    // A BeginSystem with no matching EndSystem means the caller's control flow
    // went somewhere unexpected. Reporting the time since it began -- possibly
    // seconds -- prints a number that is confidently wrong, which is worse
    // than omitting the row.
    DebugStats stats;
    stats.BeginWindow(64);
    stats.BeginFrame(0.016);
    stats.BeginSystem("never_ends");
    stats.EndFrame();
    const std::vector<std::string> lines = stats.FormatLines({}, 0);
    Assert::That(ContainsWord(lines, "never_ends"), Equals(false));
    // And it must not be smuggled in under a different name: the failure being
    // hunted is a confidently wrong number, so the row has to be absent
    // entirely rather than present and mislabelled.
    Assert::That(ContainsWord(lines, "unclosed"), Equals(false));
    Assert::That(ContainsWord(lines, "systems"), Equals(false));
  };

  It(keeps_the_most_recent_errors_and_only_those) {
    DebugStats stats;
    stats.BeginWindow(64);
    stats.Frame(0.016);
    // maxErrors 0 means "show no errors", not "show nothing" -- the rate and
    // the frame time still belong on screen.
    const std::vector<std::string> none = stats.FormatLines(RecentErrors(), 0);
    Assert::That(none.empty(), Equals(false));
    Assert::That(ContainsWord(none, "NetSocket"), Equals(false));
    // Two of three, and the two are the ones nearest the end of the log.
    const std::vector<std::string> two = stats.FormatLines(RecentErrors(), 2);
    int errorLines = 0;
    for (const std::string &line : two) {
      if (line.find("AssetStore") == std::string::npos &&
          line.find("TileMapLoader") == std::string::npos &&
          line.find("NetSocket") == std::string::npos) {
        continue;
      }
      ++errorLines;
    }
    Assert::That(errorLines, Equals(2));
  };

  It(shows_the_most_recent_errors_not_the_oldest) {
    // An overlay that shows the first two errors from a hundred-frame-old log
    // is showing the developer a bug they already fixed.
    DebugStats stats;
    stats.BeginWindow(64);
    stats.Frame(0.016);
    const std::vector<std::string> lines = stats.FormatLines(RecentErrors(), 1);
    Assert::That(ContainsWord(lines, "NetSocket"), Equals(true));
    // The strongest form: the oldest is the one that must be ABSENT. Asserting
    // only that the newest is present would also pass an implementation that
    // showed all three, which is not the claim.
    Assert::That(ContainsWord(lines, "AssetStore"), Equals(false));
    // Order matters too, not just membership: the newest error comes FIRST, so
    // a developer scanning the overlay reads it at a fixed position instead of
    // hunting for whichever line is freshest. An earlier version of this case
    // asserted membership only, and an implementation that showed the same two
    // errors oldest-first passed it.
    const std::vector<std::string> pair = stats.FormatLines(RecentErrors(), 2);
    std::size_t newestAt = pair.size();
    std::size_t olderAt = pair.size();
    for (std::size_t i = 0; i < pair.size(); ++i) {
      if (pair[i].find("NetSocket") != std::string::npos) {
        newestAt = i;
      }
      if (pair[i].find("TileMapLoader") != std::string::npos) {
        olderAt = i;
      }
    }
    Assert::That(newestAt < olderAt, Equals(true));
  };

  It(labels_the_frame_time_and_the_entity_count_it_was_given) {
    DebugStats stats;
    stats.BeginWindow(64);
    stats.Frame(0.020);
    stats.SetEntityCount(1234);
    stats.Frame(0.020);
    const std::vector<std::string> lines = stats.FormatLines({}, 0);
    Assert::That(ContainsWord(lines, "20"), Equals(true)); // ms, not seconds
    Assert::That(ContainsWord(lines, "1234"), Equals(true));
  };

  It(tolerates_a_negative_or_absurd_delta_without_printing_nonsense) {
    // A clock that jumps backwards -- which happens on a resume from suspend
    // -- produces a negative dt. A naive average then reports a negative frame
    // rate, and the overlay is the thing a developer is looking at when they
    // need to know why.
    DebugStats stats;
    stats.BeginWindow(64);
    stats.Frame(0.016);
    // LAST, deliberately. An earlier version fed the negative delta first and
    // then a good one, which meant the good frame's timestamp replaced the
    // negative one before anything was displayed -- so the case passed against
    // an implementation that printed a negative frame time, which is the whole
    // thing being hunted.
    stats.Frame(-5.0);
    Assert::That(stats.AverageFps() >= 0.0, Equals(true));
    const std::vector<std::string> lines = stats.FormatLines({}, 0);
    Assert::That(ContainsWord(lines, "-"), Equals(false));
    Assert::That(stats.LastFrameMs() >= 0.0, Equals(true));
  };

  It(toggles_without_losing_what_it_has_measured) {
    // Toggling is a visibility flag, not a reset. A developer who hides the
    // overlay to look at the game and toggles it back expects the same numbers.
    DebugOverlay overlay;
    overlay.BeginWindow(64);
    for (int i = 0; i < 10; ++i) {
      overlay.BeginFrame(0.016);
      overlay.EndFrame();
    }
    overlay.SetEntityCount(42);
    const std::vector<std::string> before = overlay.Stats().FormatLines({}, 0);
    // Hidden by default: the overlay is opt-in, so a game that constructs one
    // and forgets to wire the toggle does not ship a permanent HUD.
    Assert::That(overlay.IsVisible(), Equals(false));
    overlay.Toggle();
    Assert::That(overlay.IsVisible(), Equals(true));
    overlay.Toggle();
    Assert::That(overlay.IsVisible(), Equals(false));
    const std::vector<std::string> after = overlay.Stats().FormatLines({}, 0);
    Assert::That(after.size(), Equals(before.size()));
  };
};
