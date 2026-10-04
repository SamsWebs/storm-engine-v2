#pragma once

// The device half of the mixer. 2.8.1.
//
// The policy -- VolumeModel and ChannelPolicy -- is in mixer.h beside this
// file and is pure. This is the thin layer that actually opens a device and
// hands chunks to SDL_mixer. It is deliberately thin: the decisions live
// where they can be tested, and everything here is a call SDL_mixer already
// has.
//
// THE ENGINE DOES NOT OWN MUSIC
//
// There is no PlayMusic, no StopMusic and no FadeMusic on this class, and that
// is a decision rather than a gap. SDL_mixer's music channel is singular, and
// a game that wants a score, dynamic music or a streamed track needs to hold
// that handle itself. ApplyMusicVolume exists so such a game still plays at
// the volume VolumeModel says; the playback calls stay the game's. The
// reasoning is written out at the top of mixer.h.
//
// ORDERING, because getting it wrong is a use-after-free
//
// Close() tears down the audio device, and Mix_CloseAudio frees every open
// Mix_Chunk. AssetStore owns those chunks. So a game must clear its store
// BEFORE closing the mixer:
//
//     store.ClearAssets();   // frees the Mix_Chunk*s
//     mixer.Close();         // closes the device
//
// The reverse order hands the store freed pointers. This is not a new hazard
// -- AssetStore's own header documents the same requirement of
// TTF_CloseFont/Mix_CloseAudio -- but it is one more place to get it right,
// so it is repeated here rather than left as something to remember.
//
// Not exception-free in the sense of not throwing: nothing in this class
// throws, so it is safe to construct on a platform built with -fno-exceptions
// (Switch). It is not copyable, because holding an open device makes a copy
// mean two owners and one device.

#include <string>
#include <vector>

#include <SDL2/SDL_mixer.h>

#include "../logger.h"
#include "mixer.h"

namespace storm {

class AssetStore;

class SoundMixer {
public:
  static constexpr int kDefaultSfxChannels = 8;

  // A negative count is treated as zero, matching ChannelPolicy. The device
  // is not opened here: construct, then Open, so a game can build the mixer
  // before it knows whether it has audio.
  explicit SoundMixer(int sfxChannels = kDefaultSfxChannels);

  ~SoundMixer();

  SoundMixer(const SoundMixer &) = delete;
  SoundMixer &operator=(const SoundMixer &) = delete;

  // Opens the audio device and allocates the sfx channels. On failure it logs
  // the reason and returns false, and every other call stays safe and silent:
  // a game that runs fine without sound must not have to branch.
  //
  // If the mixer is already open this returns true without reconfiguring it.
  bool Open();

  bool IsOpen() const { return open_; }

  // Closes the device. Safe to call when never opened, and safe to call
  // twice. See the ordering note at the top of this file.
  void Close();

  // Plays a sound already loaded into a store, by asset id. Pack-aware for
  // free: the store is what decides whether the bytes came from a pack or a
  // loose file, and this only asks for the chunk.
  //
  // `store` is not owned and may be null. Returns false when the mixer is
  // closed, the store is null, the id is unknown, or the channel policy
  // declined the sound -- one boolean for all of them, because a caller that
  // asked for a sound and got silence has no useful next step.
  bool Play(const AssetStore *store, const std::string &assetId,
            SoundPriority priority = kNormalPriority);

  // The same, for a chunk the caller already holds.
  bool Play(Mix_Chunk *chunk, SoundPriority priority = kNormalPriority);

  // What the device is currently set to. The raw bus values, not the
  // multiplied ones.
  VolumeModel &Volumes() { return volumes_; }
  const VolumeModel &Volumes() const { return volumes_; }

  // Pushes the sfx volume to the device. Play() does this for you; call it
  // directly after changing Volumes() so a settings screen is heard at once.
  void ApplySfxVolume();

  // Pushes ONLY the music volume. A game that calls Mix_PlayMusic itself
  // calls this after changing Volumes().SetMusic(), which is the whole reason
  // this class does not own music.
  void ApplyMusicVolume();

  int SfxChannels() const { return policy_.SfxChannels(); }
  const ChannelPolicy &Policy() const { return policy_; }

private:
  // The bookkeeping the policy reads: one entry per sfx channel, kIdle when
  // nothing is playing. Refreshed from Mix_Playing before every pick, because
  // a channel that finished must become available again and SDL_mixer does
  // not tell the mixer which one ended unless it is given a process-wide
  // callback -- which two mixers would fight over.
  void RefreshOccupancy();

  ChannelPolicy policy_;
  VolumeModel volumes_;
  std::vector<SoundPriority> busy_;
  Logger logger;
  bool open_ = false;
};

} // namespace storm
