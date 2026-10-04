#pragma once

// Sound volume and channel policy. 2.8.1.
//
// The engine cached Mix_Chunk and stopped there. AssetStore::AddSound decodes
// and caches a sound; what happened next was every game's problem, and the
// one desktop example that has audio (netplay-checkers) shows what that costs:
//
//     Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 512);
//     if (Mix_Chunk *c = store->GetSound("move")) Mix_PlayChannel(-1, c, 0);
//     ...
//     Mix_CloseAudio();
//
// Three things are missing from that, and all three are policy rather than
// mechanism:
//
//   - **CHANNEL OWNERSHIP.** `-1` is "first free channel". The channel a sound
//     lands on is whatever happened to be free, so a burst of quiet footsteps
//     can occupy every channel and leave an explosion with nowhere to go. The
//     loud thing is the one you drop.
//   - **A VOLUME MODEL.** `0` above is the loop count, and there is no volume
//     argument at all. Nothing above SDL_mixer can turn music down, or
//     everything down at once.
//   - **Music.** SDL_mixer has one music channel and the engine says nothing
//     about it. The policy here deliberately does not either -- see below.
//
// WHY THE POLICY IS IN ITS OWN SDL-FREE HEADER
//
// This file is pure: no Mix_ call, no audio device, no fixture. That is the
// part worth testing, and it is the part that was wrong. The device half is
// SoundMixer in soundMixer.h, a thin layer over these two classes. Splitting
// them is the same arrangement common/net/hostAddress.h uses -- the RULE is
// pure, header-only and spec'd; the mechanism is a thin platform layer -- and
// it is what lets specs/audio/mixer.spec.cpp run in CI, which has no sound
// card.
//
// THE ENGINE DOES NOT OWN MUSIC
//
// There is no PlayMusic here, and that is a decision rather than an omission.
// SDL_mixer's music channel is singular, and a game that wants a score, a
// dynamic-music system or a streamed track needs to hold that handle itself.
// Owning it in the engine would mean owning it on the game's behalf, which is
// the same failure as owning the input poll: the abstraction is convenient
// until the game needs the thing it cannot get. SoundMixer therefore exposes
// MusicVolume/ApplyMusicVolume so a game calling Mix_PlayMusic itself still
// plays at the volume the model says, and the mixer never calls the music
// playback functions at all.
//
// Header-only and exception-free: no .cpp for this file, so the Switch and
// Android source lists need no new entry.

#include <cstddef>
#include <vector>

namespace storm {

// How much a sound wants a channel. Higher wins a contested one. 0..100;
// values outside the range are legal and compare as given, so a game can use
// its own scale.
using SoundPriority = int;

inline constexpr SoundPriority kLowestPriority = 0;
inline constexpr SoundPriority kNormalPriority = 50;
inline constexpr SoundPriority kHighestPriority = 100;

inline constexpr float kFullVolume = 1.0f;

enum class ChannelVerdict {
  kPlay,  // a free channel was taken
  kSteal, // `channel` was busy and is being cut off for this sound
  kDrop   // nothing to play on, and taking it would cost more than it buys
};

struct ChannelDecision {
  ChannelVerdict verdict = ChannelVerdict::kDrop;
  // Valid only when verdict is not kDrop. Always in [0, SfxChannels()).
  int channel = -1;
};

// Which sfx channel a new sound takes when they are all busy.
//
// This is the whole of channel ownership, and it is a pure function of the
// bookkeeping the caller keeps, so it is testable without a device. The
// bookkeeping is one SoundPriority per channel, kIdle for a channel with
// nothing on it; SoundMixer maintains it by asking Mix_Playing.
//
// The rules, in order, because the order is the design:
//
//   1. A FREE CHANNEL IS ALWAYS TAKEN. Contention rules must not become a
//      gate on starting -- a quiet footstep on an idle mixer is heard.
//   2. Otherwise steal the LEAST IMPORTANT busy channel, but only if it is
//      strictly less important than what is arriving.
//   3. Otherwise drop.
//
// Strictly-less in rule 2 is the load-bearing word. Letting an equal priority
// cut off its equal makes two sounds of the same class fight over one channel
// forever, with the winner decided by frame timing. Ties in "least important"
// resolve to the lowest channel index so the same frame produces the same
// choice twice.
//
// Every failure path returns kDrop rather than reading past the end. This is
// reached from audio callbacks, where a bad index is a use-after-free a frame
// later and a diagnostic nobody sees.
class ChannelPolicy {
public:
  // A negative count is treated as zero. A miscomputed count must not become
  // a huge one through a conversion.
  explicit ChannelPolicy(int sfxChannels)
      : channels_(sfxChannels > 0 ? sfxChannels : 0) {}

  int SfxChannels() const { return channels_; }

  // The bookkeeping value for a channel with nothing playing on it. Priority
  // 0 is a legal priority, so "idle" cannot be 0.
  static constexpr SoundPriority kIdle = -1;

  ChannelDecision Pick(SoundPriority incoming,
                       const std::vector<SoundPriority> &busy) const {
    if (channels_ <= 0) {
      return {ChannelVerdict::kDrop, -1};
    }
    // Longer than the policy is tolerated (extra entries are ignored);
    // shorter means the caller's bookkeeping disagrees with the configured
    // count, and guessing which is right is how a channel index gets invented.
    if (busy.size() < static_cast<std::size_t>(channels_)) {
      return {ChannelVerdict::kDrop, -1};
    }

    for (int i = 0; i < channels_; ++i) {
      if (busy[static_cast<std::size_t>(i)] == kIdle) {
        return {ChannelVerdict::kPlay, i};
      }
    }

    int least = 0;
    for (int i = 1; i < channels_; ++i) {
      if (busy[static_cast<std::size_t>(i)] <
          busy[static_cast<std::size_t>(least)]) {
        least = i;
      }
    }
    if (busy[static_cast<std::size_t>(least)] < incoming) {
      return {ChannelVerdict::kSteal, least};
    }
    return {ChannelVerdict::kDrop, -1};
  }

private:
  int channels_;
};

// Master / music / sfx, each 0..1, with the master multiplying both buses.
//
// Three buses because there are three questions a game asks: is sound on at
// all, how loud is the music, how loud are the effects. One number cannot
// answer the second and third, and per-bus numbers cannot answer the first
// without a second concept of "off".
//
// The raw bus value and the value that reaches the device are both readable.
// A slider showing "Music 80%" wants the raw one; Mix_VolumeMusic wants the
// multiplied one, and using either for the other is a bug that sounds like a
// mixer fault.
class VolumeModel {
public:
  void SetMaster(float v) { master_ = Clamp(v); }
  void SetMusic(float v) { music_ = Clamp(v); }
  void SetSfx(float v) { sfx_ = Clamp(v); }

  float Master() const { return master_; }
  float Music() const { return music_; }
  float Sfx() const { return sfx_; }

  // What the device should be given. The product of two values already in
  // [0,1] is in [0,1], so there is nothing to clamp here -- and that invariant
  // is pinned by a spec rather than left to arithmetic.
  float EffectiveMusic() const { return master_ * music_; }
  float EffectiveSfx() const { return master_ * sfx_; }

private:
  // Written as !(v > 0) rather than v < 0 so that NaN clamps to silence: NaN
  // compares false against everything, survives arithmetic unchanged, and
  // reaches an int conversion with no diagnostic anywhere.
  static float Clamp(float v) {
    if (!(v > 0.0f)) {
      return 0.0f;
    }
    return v > 1.0f ? 1.0f : v;
  }

  float master_ = kFullVolume;
  float music_ = kFullVolume;
  float sfx_ = kFullVolume;
};

} // namespace storm
