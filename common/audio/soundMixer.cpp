#include "soundMixer.h"

#include "../assetStore.h"

namespace storm {

namespace {

// SDL_mixer's volume range. The model works in 0..1 because that is what a
// slider means; this is the conversion, in one place, so no call site rounds.
int ToMixVolume(float effective) {
  if (!(effective > 0.0f)) {
    return 0;
  }
  const float scaled = effective * static_cast<float>(MIX_MAX_VOLUME);
  return static_cast<int>(scaled);
}

} // namespace

SoundMixer::SoundMixer(int sfxChannels) : policy_(sfxChannels) {
  busy_.assign(static_cast<std::size_t>(policy_.SfxChannels()),
               ChannelPolicy::kIdle);
}

SoundMixer::~SoundMixer() { Close(); }

bool SoundMixer::Open() {
  if (open_) {
    return true;
  }

  // A zero-channel mixer is legal (ChannelPolicy treats it as silence) but
  // opening a device for it would be pointless, so refuse before asking.
  if (policy_.SfxChannels() <= 0) {
    logger.Err("Open called with no sfx channels; staying closed.");
    return false;
  }

  if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 512) < 0) {
    // Deliberately not fatal. The game that calls this is expected to run
    // without sound, and a null return is what lets it.
    logger.Err(std::string("Mix_OpenAudio failed: ") + Mix_GetError());
    return false;
  }

  if (Mix_AllocateChannels(policy_.SfxChannels()) != policy_.SfxChannels()) {
    logger.Err(std::string("Mix_AllocateChannels gave ") +
               std::to_string(Mix_AllocateChannels(-1)) + " for a request of " +
               std::to_string(policy_.SfxChannels()));
    Mix_CloseAudio();
    return false;
  }

  open_ = true;
  // A fresh device starts at full volume; pushing the model over makes that
  // true by construction rather than by coincidence.
  ApplySfxVolume();
  ApplyMusicVolume();
  return true;
}

void SoundMixer::Close() {
  if (!open_) {
    return;
  }
  open_ = false;
  // Free the bookkeeping first: after this the channels belong to whoever
  // opens the device next, and ours no longer describe anything.
  std::fill(busy_.begin(), busy_.end(), ChannelPolicy::kIdle);
  Mix_CloseAudio();
}

void SoundMixer::RefreshOccupancy() {
  const int channels = policy_.SfxChannels();
  if (static_cast<int>(busy_.size()) < channels) {
    // Should be impossible: the constructor sized it from the policy and
    // neither is assignable. Growing here rather than trusting it, because a
    // short vector is exactly what Pick() refuses to read past.
    busy_.assign(static_cast<std::size_t>(channels), ChannelPolicy::kIdle);
  }
  for (int i = 0; i < channels; ++i) {
    const std::size_t at = static_cast<std::size_t>(i);
    if (Mix_Playing(i) == 0) {
      busy_[at] = ChannelPolicy::kIdle;
    }
    // A channel that is still playing keeps the priority we gave it: that is
    // what makes a steal decision meaningful a second later.
  }
}

bool SoundMixer::Play(Mix_Chunk *chunk, SoundPriority priority) {
  if (!open_ || chunk == nullptr) {
    return false;
  }

  RefreshOccupancy();
  const ChannelDecision decision = policy_.Pick(priority, busy_);
  if (decision.verdict == ChannelVerdict::kDrop) {
    return false;
  }

  if (Mix_PlayChannel(decision.channel, chunk, 0) < 0) {
    logger.Err(std::string("Mix_PlayChannel failed: ") + Mix_GetError());
    return false;
  }

  busy_[static_cast<std::size_t>(decision.channel)] = priority;
  return true;
}

bool SoundMixer::Play(const AssetStore *store, const std::string &assetId,
                      SoundPriority priority) {
  if (store == nullptr) {
    return false;
  }
  // Pack-aware for free: the store is what knows whether these bytes came
  // from a pack or a loose file, and this only asks for the chunk.
  return Play(store->GetSound(assetId), priority);
}

void SoundMixer::ApplySfxVolume() {
  if (!open_) {
    return;
  }
  // -1 is every channel SDL_mixer allocated, which is exactly the set this
  // mixer owns.
  Mix_Volume(-1, ToMixVolume(volumes_.EffectiveSfx()));
}

void SoundMixer::ApplyMusicVolume() {
  if (!open_) {
    return;
  }
  // The music channel is the game's to play, but its volume is the model's
  // to set -- otherwise a game holding its own Mix_Music * still cannot be
  // quietened by the same settings screen.
  Mix_VolumeMusic(ToMixVolume(volumes_.EffectiveMusic()));
}

} // namespace storm
