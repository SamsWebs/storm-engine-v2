// The mixer's policy — 2.8.1.
//
// The engine cached Mix_Chunk and nothing else: AssetStore::AddSound decodes
// and caches, and every game that wanted a sound reached for
// `Mix_PlayChannel(-1, chunk, 0)` itself. `-1` means "first free channel", so
// the channel a sound lands on is whatever happened to be free, and there is
// no volume model anywhere above SDL_mixer.
//
// These cases are about the POLICY — which channel a sound takes when they all
// are busy, and what master/music/sfx volume multiply to. That is the part
// worth testing, and it is pure: no audio device, no Mix_ call, no fixture.
//
// The SDL half (Open/Close, actually playing the chunk) is a thin layer over
// this and is deliberately not spec'd here, for the reason
// specs/tilemapFormat.spec.cpp records for the map reader: a case that needs
// hardware to run is a case that will silently stop running.
#include <igloo/igloo_alt.h>

#include <limits>
#include <vector>

#include "../../common/audio/mixer.h"

using namespace igloo;
using namespace storm;

namespace {

// The shape the SDL layer keeps: one entry per sfx channel, kIdle when free.
std::vector<SoundPriority> Busy(std::initializer_list<SoundPriority> xs) {
  return std::vector<SoundPriority>(xs);
}

} // namespace

Describe(ChannelPolicySpec){

    It(takes_a_free_channel_whatever_the_incoming_priority){
        // A quiet footstep on an idle mixer must still be heard. The steal rule
        // below is about contention only; it must not become a gate on
        // starting.
        const ChannelPolicy policy(4);
const auto d = policy.Pick(kLowestPriority,
                           Busy({ChannelPolicy::kIdle, ChannelPolicy::kIdle,
                                 ChannelPolicy::kIdle, ChannelPolicy::kIdle}));
Assert::That(d.verdict, Equals(ChannelVerdict::kPlay));
Assert::That(d.channel, Equals(0));
}

It(returns_the_lowest_free_channel_so_the_choice_is_deterministic) {
  // Two runs of the same frame must not scatter the same sounds across
  // different channels; a spec that cannot say WHICH channel is only
  // asserting that some channel was returned.
  const ChannelPolicy policy(4);
  const auto d =
      policy.Pick(kNormalPriority, Busy({ChannelPolicy::kIdle, 50, 50, 50}));
  Assert::That(d.verdict, Equals(ChannelVerdict::kPlay));
  Assert::That(d.channel, Equals(0));
}

It(steals_the_least_important_channel_when_the_incoming_sound_outranks_it) {
  // The bug this exists for. With Mix_PlayChannel(-1, ...) a low-priority
  // sound can be sitting on every channel and the explosion has nowhere to
  // go, so it is the footstep that is heard and the explosion is not.
  const ChannelPolicy policy(3);
  const auto d = policy.Pick(kHighestPriority, Busy({10, 30, 20}));
  Assert::That(d.verdict, Equals(ChannelVerdict::kSteal));
  Assert::That(d.channel, Equals(0)); // priority 10, the lowest of the three
}

It(drops_rather_than_stealing_from_an_equal_priority) {
  // Equal priority must NOT cut the older sound off. Letting it do so makes
  // two sounds of the same class fight over one channel forever, and which
  // one wins is then a function of frame timing.
  const ChannelPolicy policy(2);
  const auto d =
      policy.Pick(kNormalPriority, Busy({kNormalPriority, kNormalPriority}));
  Assert::That(d.verdict, Equals(ChannelVerdict::kDrop));
}

It(drops_rather_than_stealing_from_a_higher_priority) {
  const ChannelPolicy policy(2);
  const auto d =
      policy.Pick(kLowestPriority, Busy({kHighestPriority, kNormalPriority}));
  Assert::That(d.verdict, Equals(ChannelVerdict::kDrop));
}

It(steals_the_lowest_channel_index_when_priorities_tie) {
  // Determinism again: a tie must not resolve by iteration order.
  const ChannelPolicy policy(4);
  const auto d = policy.Pick(kHighestPriority, Busy({70, 70, 70, 70}));
  Assert::That(d.verdict, Equals(ChannelVerdict::kSteal));
  Assert::That(d.channel, Equals(0));
}

It(never_reports_a_channel_it_was_not_given) {
  // A mixer configured for one channel must not return channel 1 just
  // because the caller's bookkeeping vector is longer than the policy.
  const ChannelPolicy policy(1);
  const auto d = policy.Pick(
      kNormalPriority,
      Busy({ChannelPolicy::kIdle, ChannelPolicy::kIdle, ChannelPolicy::kIdle}));
  Assert::That(d.verdict, Equals(ChannelVerdict::kPlay));
  Assert::That(d.channel, Equals(0));
}

It(drops_everything_when_no_channels_were_allocated) {
  // A game that never opened audio gets silence, not a crash and not a
  // channel number it does not own.
  const ChannelPolicy policy(0);
  const auto d = policy.Pick(kHighestPriority, Busy({}));
  Assert::That(d.verdict, Equals(ChannelVerdict::kDrop));
}

It(drops_rather_than_reading_a_busy_vector_it_does_not_own) {
  // Too SHORT, which is the direction that reads past the end. Failing
  // closed is the whole point: this is a path reached from an audio
  // callback, where returning garbage is a use-after-free one frame later.
  const ChannelPolicy policy(4);
  const auto d = policy.Pick(kHighestPriority, Busy({50, 50}));
  Assert::That(d.verdict, Equals(ChannelVerdict::kDrop));
}

It(keeps_the_channel_count_it_was_given) {
  // SDL_mixer's Mix_AllocateChannels is the ceiling, and the policy must not
  // hand out a channel the mixer never asked for.
  Assert::That(ChannelPolicy(8).SfxChannels(), Equals(8));
  Assert::That(ChannelPolicy(0).SfxChannels(), Equals(0));
}

It(fails_closed_on_a_negative_channel_count) {
  // A miscomputed count must not become a huge unsigned one.
  Assert::That(ChannelPolicy(-4).SfxChannels(), Equals(0));
}
}
;

Describe(VolumeModelSpec){

    It(plays_at_full_volume_by_default){const VolumeModel v;
Assert::That(v.Master(), Equals(1.0f));
Assert::That(v.EffectiveMusic(), Equals(1.0f));
Assert::That(v.EffectiveSfx(), Equals(1.0f));
}

It(multiplies_master_by_the_bus_it_gates) {
  // One number has to be able to mute everything, which is the case a
  // per-bus-only model cannot express.
  VolumeModel v;
  v.SetSfx(0.5f);
  v.SetMusic(0.25f);
  v.SetMaster(0.5f);
  Assert::That(v.EffectiveSfx(), Equals(0.25f));
  Assert::That(v.EffectiveMusic(), Equals(0.125f));
}

It(mutes_both_buses_at_once_when_the_master_is_zero) {
  VolumeModel v;
  v.SetMaster(0.0f);
  Assert::That(v.EffectiveSfx(), Equals(0.0f));
  Assert::That(v.EffectiveMusic(), Equals(0.0f));
}

It(clamps_a_volume_above_one_instead_of_amplifying) {
  // Mix_Volume takes an int out of 0..128. A game passing 2.0 should get
  // "as loud as possible", not a value the mixer has to clamp itself.
  VolumeModel v;
  v.SetSfx(4.0f);
  Assert::That(v.Sfx(), Equals(1.0f));
  Assert::That(v.EffectiveSfx(), Equals(1.0f));
}

It(clamps_a_negative_volume_to_silence_rather_than_inverting_it) {
  // -1 would come out of "master minus this slider" arithmetic. Inverting
  // the mix is worse than muting it.
  VolumeModel v;
  v.SetSfx(-3.0f);
  Assert::That(v.Sfx(), Equals(0.0f));
  Assert::That(v.EffectiveSfx(), Equals(0.0f));
}

It(treats_a_nan_volume_as_silence_rather_than_poisoning_both_buses) {
  // NaN survives arithmetic silently: NaN * 0.5 is NaN, and it reaches
  // Mix_Volume as an int conversion with no diagnostic. Clamping it to 0
  // costs a stray sound; letting it through costs an undefined one.
  const float nan = std::numeric_limits<float>::quiet_NaN();
  VolumeModel v;
  v.SetSfx(nan);
  Assert::That(v.Sfx(), Equals(0.0f));
  Assert::That(v.EffectiveSfx(), Equals(0.0f));
}

It(keeps_the_buses_independent) {
  // Moving one must not disturb another, or a music slider quietly changes
  // the effects volume and nobody can tell why.
  VolumeModel v;
  v.SetMusic(0.3f);
  Assert::That(v.Sfx(), Equals(1.0f));
  v.SetSfx(0.7f);
  Assert::That(v.Music(), Equals(0.3f));
  Assert::That(v.Sfx(), Equals(0.7f));
}

It(reports_the_bus_as_set_separately_from_what_reaches_the_device) {
  // Master is a ceiling over both. A game showing "Music: 80%" on a slider
  // needs the raw value, not the multiplied one.
  VolumeModel v;
  v.SetMusic(0.8f);
  v.SetMaster(0.5f);
  Assert::That(v.Music(), Equals(0.8f));
  Assert::That(v.EffectiveMusic(), Equals(0.4f));
}

It(never_produces_an_effective_volume_outside_zero_to_one) {
  // The product of two clamped values is already in range, so this is a
  // pin on the invariant rather than a test of arithmetic: it is the thing
  // a later "let the master exceed 1 for a boost" change would break.
  for (float m = -1.0f; m <= 2.0f; m += 0.25f) {
    for (float b = -1.0f; b <= 2.0f; b += 0.25f) {
      VolumeModel v;
      v.SetMaster(m);
      v.SetSfx(b);
      const float e = v.EffectiveSfx();
      Assert::That(e >= 0.0f, Equals(true));
      Assert::That(e <= 1.0f, Equals(true));
    }
  }
}
}
;
