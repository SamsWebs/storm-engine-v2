#pragma once

// A diagnostic overlay a developer can read DURING play.
//
// 2.0.0's diagnostics reach a log nobody reads while playing. This is the only
// diagnostic surface a game actually shows a player, and it is opt-in, so the
// cost when nobody wants it is one branch per frame.
//
// WHY THE NUMBERS ARE PURE AND TAKE THEIR CLOCK FROM THE CALLER
//
// DebugStats never calls SDL_GetTicks. It is handed the frame's delta and the
// per-system durations, and it decides what to do with them. If the clock were
// inside, every case in specs/debugOverlay.spec.cpp would be a timing
// assertion, and a timing assertion is a flaky assertion that gets deleted
// within a month. This way the fps arithmetic, the window, the system names and
// the error tail are all testable exactly.
//
// WHY THE GAME OWNS THE OVERLAY AND REGISTERS IT
//
// DebugOverlay is a plain object, not a GameState member and not a singleton.
// A game holds one, and registers it in the OverlayList it already has for
// 2.4.1 -- so the overlay is drawn BEFORE the present, in order, and cannot end
// up on the wrong side of it. That is the whole reason 2.4.1 exists, and this
// is its first real consumer:
//
//     storm::OverlayList overlays;
//     storm::DebugOverlay debug;
//     overlays.Add("debug", [&debug](SDL_Renderer *r) {
//       debug.Draw(r, font, 8, 8, storm::Logger::messages);
//     });
//     // ...and in render():  Present(renderer_, overlays);
//
// The overlay does not poll for the toggle key. The engine's input layer is
// edge-triggered and deliberately does not poll -- the active state owns all
// polling -- so the state calls Toggle() from its own processInput().
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <string>
#include <vector>

#include "logger.h"
#include "text.h"

namespace storm {

// The measured half. No SDL, no clock, no drawing.
class DebugStats {
public:
  // How many frames the fps average spans. 64 is about a second at 60fps:
  // long enough that one hitch does not dominate, short enough that the number
  // still responds while a developer is watching for a change.
  static constexpr int kDefaultWindow = 64;

  // A window is set up once and does not have to be re-set per frame. Naming it
  // is the caller's job because the whole point is that nothing here reads a
  // clock: BeginWindow(kDefaultWindow) is the usual call.
  void BeginWindow(int frames) {
    m_window = frames > 0 ? frames : 1;
    m_deltas.clear();
    m_elapsed = 0.0;
  }

  // One frame's delta, in seconds. Negative values -- a clock that jumped
  // backwards on resume from suspend -- are counted as zero rather than
  // subtracted, because a negative frame rate is a number nobody can reason
  // about and the overlay is what a developer is looking at precisely when they
  // need to know why.
  void Frame(double deltaSeconds) {
    if (m_window <= 0) {
      BeginWindow(kDefaultWindow);
    }
    const double d = deltaSeconds > 0.0 ? deltaSeconds : 0.0;
    m_deltas.push_back(d);
    m_elapsed += d;
    while (static_cast<int>(m_deltas.size()) > m_window) {
      // A ring, not a running total: a single 200ms hitch has to age out, or
      // the reported rate never recovers and the developer turns the overlay
      // off and never turns it on again.
      m_elapsed -= m_deltas.front();
      m_deltas.erase(m_deltas.begin());
    }
    m_lastFrameMs = d * 1000.0;
  }

  // Average over the samples actually fed, not over the window size. Dividing
  // by the full window on frame one reports 1/64th of the real rate, so a
  // freshly toggled overlay opens showing a number that is wrong by a factor
  // of how many frames it has seen.
  double AverageFps() const {
    if (m_deltas.empty() || m_elapsed <= 0.0) {
      return 0.0;
    }
    return static_cast<double>(m_deltas.size()) / m_elapsed;
  }

  double LastFrameMs() const { return m_lastFrameMs; }

  void SetEntityCount(int count) { m_entityCount = count; }
  int EntityCount() const { return m_entityCount; }

  // Per-system timing for ONE frame. Begin/End rather than a single call so a
  // system that is conditionally skipped can simply not call End, and so nested
  // or re-entrant measurement is at least visible in the code.
  void BeginFrame(double deltaSeconds) {
    Frame(deltaSeconds);
    m_systemMs.clear();
    m_openSystem.clear();
  }

  void BeginSystem(const std::string &name) { m_openSystem = name; }

  // Milliseconds this system took. An unmatched End (no Begin, or a second End
  // for the same Begin) is ignored rather than guessed at, and an unclosed
  // Begin is dropped at EndFrame -- printing the time since it opened, possibly
  // seconds, is a confidently wrong number, which is worse than no row.
  void EndSystem(double milliseconds) {
    if (m_openSystem.empty() || milliseconds < 0.0) {
      return;
    }
    for (auto &entry : m_systemMs) {
      if (entry.first == m_openSystem) {
        entry.second += milliseconds;
        m_openSystem.clear();
        return;
      }
    }
    // A vector rather than a map, deliberately: registration order is
    // deterministic, so the overlay's rows do not reshuffle between frames as
    // a hash map's would, and the number of systems in a frame is small enough
    // that the linear scan is not the thing worth optimising.
    m_systemMs.emplace_back(m_openSystem, milliseconds);
    m_openSystem.clear();
  }

  void EndFrame() { m_openSystem.clear(); }

  const std::vector<std::pair<std::string, double>> &SystemTimings() const {
    return m_systemMs;
  }

  // The lines to draw, most useful first: rate, frame time, entity count, the
  // SLOWEST system (which is the question per-system timings exist to answer),
  // then the last `maxErrors` errors -- the most RECENT ones, since an overlay
  // showing a hundred-frame-old error is showing a bug that is already fixed.
  std::vector<std::string> FormatLines(const std::vector<LogEntry> &errors,
                                       int maxErrors) const {
    std::vector<std::string> lines;
    if (m_deltas.empty()) {
      // Nothing measured yet. Returning a plausible-looking zero is worse than
      // returning nothing: the overlay opens on frame one, often.
      return lines;
    }

    lines.push_back(Format("%.1f fps  %.1f ms", AverageFps(), m_lastFrameMs));
    if (m_entityCount >= 0) {
      lines.push_back(Format("entities %d", m_entityCount));
    }

    if (!m_systemMs.empty()) {
      double total = 0.0;
      std::string slowest;
      double slowestMs = -1.0;
      for (const auto &entry : m_systemMs) {
        total += entry.second;
        if (entry.second > slowestMs) {
          slowestMs = entry.second;
          slowest = entry.first;
        }
      }
      lines.push_back(Format("systems %.2f ms, slowest %s %.2f ms", total,
                             slowest.c_str(), slowestMs));
    }

    if (maxErrors > 0 && !errors.empty()) {
      const std::size_t from =
          errors.size() > static_cast<std::size_t>(maxErrors)
              ? errors.size() - static_cast<std::size_t>(maxErrors)
              : 0;
      for (std::size_t i = errors.size(); i > from; --i) {
        lines.push_back(errors[i - 1].message);
      }
    }
    return lines;
  }

private:
  static std::string Format(const char *fmt, ...) {
    char buffer[512];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    return std::string(buffer);
  }

  int m_window = 0;
  std::vector<double> m_deltas;
  double m_elapsed = 0.0;
  double m_lastFrameMs = 0.0;
  int m_entityCount = -1;
  std::vector<std::pair<std::string, double>> m_systemMs;
  std::string m_openSystem;
};

// The visible half: a DebugStats plus a flag and a Draw().
class DebugOverlay {
public:
  void BeginWindow(int frames) { m_stats.BeginWindow(frames); }
  void BeginFrame(double deltaSeconds) { m_stats.BeginFrame(deltaSeconds); }
  void BeginSystem(const std::string &name) { m_stats.BeginSystem(name); }
  void EndSystem(double ms) { m_stats.EndSystem(ms); }
  void EndFrame() { m_stats.EndFrame(); }
  void SetEntityCount(int count) { m_stats.SetEntityCount(count); }

  // A visibility flag, not a reset. A developer who hides the overlay to look
  // at the game and toggles it back expects the same numbers they had.
  void Toggle() { m_visible = !m_visible; }
  bool IsVisible() const { return m_visible; }
  void SetVisible(bool visible) { m_visible = visible; }

  DebugStats &Stats() { return m_stats; }
  const DebugStats &Stats() const { return m_stats; }

  // Draws top-left at (x, y). A null renderer or font draws nothing and leaks
  // nothing -- the same contract Text has, because this is called from inside a
  // frame that must not be taken down by a diagnostic.
  //
  // The half-open design is deliberate: the caller registers this in an
  // OverlayList, so the lines are drawn BEFORE SDL_RenderPresent. An overlay
  // drawn after the present is an overlay nobody ever sees, which is a
  // diagnostic that is worse than none because it looks configured.
  void Draw(SDL_Renderer *renderer, TTF_Font *font, int x, int y,
            const std::vector<LogEntry> &errors = Logger::messages,
            int maxErrors = 5, int lineHeight = 0) const {
    if (!m_visible || !renderer || !font) {
      return;
    }
    const std::vector<std::string> lines =
        m_stats.FormatLines(errors, maxErrors);
    if (lines.empty()) {
      return;
    }
    const int step = lineHeight > 0 ? lineHeight : Text::Measure(font, "Ag").y;
    const SDL_Color dim{200, 200, 200, 255};
    for (std::size_t i = 0; i < lines.size(); ++i) {
      Text::Draw(renderer, font, lines[i], x, y + static_cast<int>(i) * step,
                 dim);
    }
  }

private:
  DebugStats m_stats;
  bool m_visible = false;
};

} // namespace storm
