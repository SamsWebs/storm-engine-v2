#include <cstring>

#include <igloo/igloo_alt.h>

#include "../../common/net/netSnapshot.h"

using namespace igloo;
using namespace storm;

namespace {
void AddSkater(NetSnapshot &snap, int id, int x, int y, int energy) {
  int32_t data[3] = {x, y, energy};
  Assert::That(snap.AddItem(1, (uint16_t)id, data, 3), Equals(true));
}
void AddPuck(NetSnapshot &snap, int x, int y) {
  int32_t data[2] = {x, y};
  Assert::That(snap.AddItem(2, 0, data, 2), Equals(true));
}
} // namespace

Describe(NetSnapshotSpec) {

  Describe(Builder) {
    It(should_sort_items_by_key_on_finish) {
      NetSnapshot snap;
      AddPuck(snap, 10, 20);       // key (2<<16)
      AddSkater(snap, 5, 1, 2, 3); // key (1<<16)|5
      AddSkater(snap, 1, 4, 5, 6); // key (1<<16)|1
      snap.Finish();
      Assert::That(snap.NumItems(), Equals(3));
      uint16_t type = 0, id = 0;
      const int32_t *data = nullptr;
      int count = 0;
      Assert::That(snap.GetItemByIndex(0, type, id, data, count), Equals(true));
      Assert::That(type, Equals(1)); // skater id 1 sorts before id 5
      Assert::That(id, Equals(1));
    };
    It(should_report_correct_counts_for_out_of_order_inserts) {
      NetSnapshot snap;
      int32_t skater[3] = {1, 2, 3};
      int32_t puck[2] = {10, 20};
      int32_t spek[4] = {5, 6, 7, 8};
      Assert::That(snap.AddItem(1, 1, skater, 3), Equals(true));
      Assert::That(snap.AddItem(2, 0, puck, 2), Equals(true));
      Assert::That(snap.AddItem(1, 3, skater, 3), Equals(true));
      Assert::That(snap.AddItem(3, 0, spek, 4), Equals(true));
      snap.Finish();
      Assert::That(snap.NumItems(), Equals(4));
      uint16_t type = 0, id = 0;
      const int32_t *data = nullptr;
      int count = 0;
      Assert::That(snap.GetItemByIndex(0, type, id, data, count), Equals(true));
      Assert::That(count, Equals(3)); // skater(1,1) — never 5/-2
      Assert::That(snap.GetItemByIndex(1, type, id, data, count), Equals(true));
      Assert::That(type, Equals(1));
      Assert::That(id, Equals(3));
      Assert::That(count, Equals(3));
      Assert::That(snap.GetItemByIndex(2, type, id, data, count), Equals(true));
      Assert::That(type, Equals(2));
      Assert::That(count, Equals(2));
      Assert::That(snap.GetItemByIndex(3, type, id, data, count), Equals(true));
      Assert::That(type, Equals(3));
      Assert::That(count, Equals(4));
      const int32_t *found = nullptr;
      Assert::That(snap.FindItem(1, 1, found, count), Equals(true));
      Assert::That(count, Equals(3));
      Assert::That(snap.FindItem(2, 0, found, count), Equals(true));
      Assert::That(count, Equals(2));
      Assert::That(snap.FindItem(3, 0, found, count), Equals(true));
      Assert::That(count, Equals(4));
    };
    It(should_keep_crc_stable_across_different_insertion_orders) {
      int32_t skater[3] = {10, 20, 30};
      int32_t puck[2] = {5, 6};
      int32_t spek[1] = {99};
      NetSnapshot a; // type 2 first, then 1, then 3
      Assert::That(a.AddItem(2, 0, puck, 2), Equals(true));
      Assert::That(a.AddItem(1, 7, skater, 3), Equals(true));
      Assert::That(a.AddItem(3, 1, spek, 1), Equals(true));
      a.Finish();
      NetSnapshot b; // type 3 first, then 1, then 2
      Assert::That(b.AddItem(3, 1, spek, 1), Equals(true));
      Assert::That(b.AddItem(1, 7, skater, 3), Equals(true));
      Assert::That(b.AddItem(2, 0, puck, 2), Equals(true));
      b.Finish();
      Assert::That(a.Crc(), Equals(b.Crc()));
    };
    It(should_round_trip_delta_snapshots_built_out_of_order) {
      // The exact shape docs/networking.md recommends: puck added before the
      // skaters, mixed counts — pre-fix the negative count killed replication.
      NetSnapshot from;
      from.Finish();
      NetSnapshot to;
      int32_t puck[2] = {7, 8};
      int32_t skaterA[3] = {1, 2, 3};
      int32_t skaterB[4] = {4, 5, 6, 7};
      Assert::That(to.AddItem(2, 0, puck, 2), Equals(true));
      Assert::That(to.AddItem(1, 1, skaterA, 3), Equals(true));
      Assert::That(to.AddItem(1, 2, skaterB, 4), Equals(true));
      to.Finish();
      uint8_t delta[256];
      int n = NetSnapshotDelta::Create(from, to, delta, sizeof(delta));
      Assert::That(n, Is().GreaterThan(0));
      NetSnapshot rebuilt;
      Assert::That(NetSnapshotDelta::Apply(from, delta, n, rebuilt),
                   Equals(true));
      Assert::That(rebuilt.Crc(), Equals(to.Crc()));
      Assert::That(rebuilt.NumItems(), Equals(3));
    };
    It(should_find_items_after_finish) {
      NetSnapshot snap;
      AddSkater(snap, 9, 100, 200, 50);
      AddPuck(snap, 300, 400);
      snap.Finish();
      const int32_t *data = nullptr;
      int count = 0;
      Assert::That(snap.FindItem(1, 9, data, count), Equals(true));
      Assert::That(count, Equals(3));
      Assert::That(data[0], Equals(100));
      Assert::That(data[2], Equals(50));
      Assert::That(snap.FindItem(2, 0, data, count), Equals(true));
      Assert::That(count, Equals(2));
      Assert::That(snap.FindItem(1, 99, data, count), Equals(false));
    };
    It(should_replace_duplicate_keys) {
      NetSnapshot snap;
      AddSkater(snap, 1, 1, 1, 1);
      int32_t data[3] = {9, 9, 9};
      Assert::That(snap.AddItem(1, 1, data, 3), Equals(true)); // replace
      snap.Finish();
      Assert::That(snap.NumItems(), Equals(1));
      const int32_t *out = nullptr;
      int count = 0;
      Assert::That(snap.FindItem(1, 1, out, count), Equals(true));
      Assert::That(out[0], Equals(9));
    };
    It(should_refuse_adds_after_finish) {
      NetSnapshot snap;
      AddPuck(snap, 0, 0);
      snap.Finish();
      int32_t data[1] = {1};
      Assert::That(snap.AddItem(3, 0, data, 1), Equals(false));
    };
    It(should_compute_a_stable_crc) {
      NetSnapshot a;
      AddSkater(a, 3, 10, 20, 30);
      AddPuck(a, 5, 6);
      a.Finish();
      NetSnapshot b;
      AddSkater(b, 3, 10, 20, 30);
      AddPuck(b, 5, 6);
      b.Finish();
      Assert::That(a.Crc(), Equals(b.Crc()));
    };
    It(should_change_the_crc_when_data_changes) {
      NetSnapshot a;
      AddPuck(a, 5, 6);
      a.Finish();
      NetSnapshot b;
      AddPuck(b, 5, 7);
      b.Finish();
      Assert::That(a.Crc(), Is().Not().EqualTo(b.Crc()));
    };
  };

  Describe(Delta) {
    It(should_encode_changed_items_as_diffs) {
      NetSnapshot from;
      AddSkater(from, 1, 10, 20, 100);
      from.Finish();
      NetSnapshot to;
      AddSkater(to, 1, 12, 20, 95); // x +2, energy -5
      to.Finish();
      uint8_t delta[128];
      int n = NetSnapshotDelta::Create(from, to, delta, sizeof(delta));
      Assert::That(n, Is().GreaterThan(0));
      NetSnapshot rebuilt;
      Assert::That(NetSnapshotDelta::Apply(from, delta, n, rebuilt),
                   Equals(true));
      Assert::That(rebuilt.Crc(), Equals(to.Crc()));
    };
    It(should_encode_new_items_as_absolutes) {
      NetSnapshot from;
      from.Finish();
      NetSnapshot to;
      AddSkater(to, 2, 30, 40, 80);
      AddPuck(to, 1, 2);
      to.Finish();
      uint8_t delta[256];
      int n = NetSnapshotDelta::Create(from, to, delta, sizeof(delta));
      Assert::That(n, Is().GreaterThan(0));
      NetSnapshot rebuilt;
      Assert::That(NetSnapshotDelta::Apply(from, delta, n, rebuilt),
                   Equals(true));
      Assert::That(rebuilt.Crc(), Equals(to.Crc()));
      Assert::That(rebuilt.NumItems(), Equals(2));
    };
    It(should_encode_deleted_items) {
      NetSnapshot from;
      AddSkater(from, 1, 1, 1, 1);
      AddPuck(from, 2, 2);
      from.Finish();
      NetSnapshot to;
      AddPuck(to, 2, 2);
      to.Finish();
      uint8_t delta[128];
      int n = NetSnapshotDelta::Create(from, to, delta, sizeof(delta));
      Assert::That(n, Is().GreaterThan(0));
      NetSnapshot rebuilt;
      Assert::That(NetSnapshotDelta::Apply(from, delta, n, rebuilt),
                   Equals(true));
      Assert::That(rebuilt.NumItems(), Equals(1));
      const int32_t *data = nullptr;
      int count = 0;
      Assert::That(rebuilt.FindItem(1, 1, data, count), Equals(false));
      Assert::That(rebuilt.FindItem(2, 0, data, count), Equals(true));
    };
    It(should_return_zero_when_nothing_changed) {
      NetSnapshot from;
      AddSkater(from, 1, 10, 20, 100);
      from.Finish();
      NetSnapshot to;
      AddSkater(to, 1, 10, 20, 100);
      to.Finish();
      uint8_t delta[128];
      Assert::That(NetSnapshotDelta::Create(from, to, delta, sizeof(delta)),
                   Equals(0));
    };
    It(should_reject_malformed_deltas) {
      NetSnapshot from;
      AddSkater(from, 1, 1, 1, 1);
      from.Finish();
      NetSnapshot out;
      uint8_t garbage[] = {0x7F, 0x7F, 0x7F, 0x7F, 0x7F}; // huge counts
      Assert::That(NetSnapshotDelta::Apply(from, garbage, sizeof(garbage), out),
                   Equals(false));
    };
    It(should_apply_a_mixed_delta) {
      NetSnapshot from;
      AddSkater(from, 1, 10, 20, 100);
      AddSkater(from, 2, 50, 60, 70);
      from.Finish();
      NetSnapshot to;
      AddSkater(to, 1, 10, 21, 100); // changed
      AddSkater(to, 2, 50, 60, 70);  // unchanged
      AddSkater(to, 3, 99, 98, 97);  // new
      to.Finish();
      uint8_t delta[256];
      int n = NetSnapshotDelta::Create(from, to, delta, sizeof(delta));
      Assert::That(n, Is().GreaterThan(0));
      NetSnapshot rebuilt;
      Assert::That(NetSnapshotDelta::Apply(from, delta, n, rebuilt),
                   Equals(true));
      Assert::That(rebuilt.Crc(), Equals(to.Crc()));
    };
    It(should_round_trip_items_that_grow) {
      NetSnapshot from;
      int32_t base[2] = {10, 20};
      from.AddItem(1, 1, base, 2);
      from.Finish();
      NetSnapshot to;
      int32_t grown[3] = {12, 22, 30}; // same item, now 3 ints
      to.AddItem(1, 1, grown, 3);
      to.Finish();
      uint8_t delta[128];
      int n = NetSnapshotDelta::Create(from, to, delta, sizeof(delta));
      Assert::That(n, Is().GreaterThan(0));
      NetSnapshot rebuilt;
      Assert::That(NetSnapshotDelta::Apply(from, delta, n, rebuilt),
                   Equals(true));
      Assert::That(rebuilt.Crc(), Equals(to.Crc()));
      const int32_t *data = nullptr;
      int count = 0;
      Assert::That(rebuilt.FindItem(1, 1, data, count), Equals(true));
      Assert::That(count, Equals(3));
      Assert::That(data[0], Equals(12));
      Assert::That(data[1], Equals(22));
      Assert::That(data[2], Equals(30));
    };
    It(should_round_trip_items_that_shrink) {
      NetSnapshot from;
      int32_t base[3] = {10, 20, 30};
      from.AddItem(1, 1, base, 3);
      from.Finish();
      NetSnapshot to;
      int32_t shrunk[2] = {15, 25}; // same item, now 2 ints
      to.AddItem(1, 1, shrunk, 2);
      to.Finish();
      uint8_t delta[128];
      int n = NetSnapshotDelta::Create(from, to, delta, sizeof(delta));
      Assert::That(n, Is().GreaterThan(0));
      NetSnapshot rebuilt;
      Assert::That(NetSnapshotDelta::Apply(from, delta, n, rebuilt),
                   Equals(true));
      Assert::That(rebuilt.Crc(), Equals(to.Crc()));
      const int32_t *data = nullptr;
      int count = 0;
      Assert::That(rebuilt.FindItem(1, 1, data, count), Equals(true));
      Assert::That(count, Equals(2));
      Assert::That(data[0], Equals(15));
      Assert::That(data[1], Equals(25));
    };
    It(should_round_trip_count_changes_mixed_with_other_updates) {
      NetSnapshot from;
      AddSkater(from, 1, 10, 20, 100); // will grow to 4 ints
      AddPuck(from, 2, 2);             // will be deleted
      AddSkater(from, 3, 7, 7, 7);     // will shrink to 1 int
      from.Finish();
      NetSnapshot to;
      int32_t grown[4] = {12, 22, 30, 40};
      to.AddItem(1, 1, grown, 4);
      int32_t shrunk[1] = {99};
      to.AddItem(1, 3, shrunk, 1);
      int32_t puck[2] = {2, 9}; // new
      to.AddItem(2, 0, puck, 2);
      to.Finish();
      uint8_t delta[256];
      int n = NetSnapshotDelta::Create(from, to, delta, sizeof(delta));
      Assert::That(n, Is().GreaterThan(0));
      NetSnapshot rebuilt;
      Assert::That(NetSnapshotDelta::Apply(from, delta, n, rebuilt),
                   Equals(true));
      Assert::That(rebuilt.Crc(), Equals(to.Crc()));
      Assert::That(rebuilt.NumItems(), Equals(3));
    };
    It(should_refuse_in_place_replaces_with_a_different_count) {
      NetSnapshot snap;
      int32_t base[2] = {1, 2};
      snap.AddItem(1, 1, base, 2);
      int32_t bigger[3] = {1, 2, 3};
      Assert::That(snap.AddItem(1, 1, bigger, 3),
                   Equals(false)); // no silent corruption
    };
    It(should_estimate_size_bounds) {
      NetSnapshot from;
      AddSkater(from, 1, 10, 20, 100);
      from.Finish();
      NetSnapshot to;
      AddSkater(to, 1, 10, 21, 100);
      AddSkater(to, 2, 50, 60, 70);
      to.Finish();
      // A fixed buffer, not uint8_t delta[EstimateSize(from, to)]: that is a
      // C99 VLA, a GCC extension MSVC rejects, and docs/networking.md used to
      // teach it. kNetMaxChunkSize is the real ceiling anyway — a delta larger
      // than one chunk cannot be sent (P36).
      uint8_t delta[kNetMaxChunkSize];
      Assert::That(NetSnapshotDelta::EstimateSize(from, to),
                   Is().LessThanOrEqualTo((int)sizeof(delta)));
      int n = NetSnapshotDelta::Create(from, to, delta, sizeof(delta));
      Assert::That(n, Is().GreaterThanOrEqualTo(0));
      Assert::That(
          n, Is().LessThanOrEqualTo(NetSnapshotDelta::EstimateSize(from, to)));
    };
    It(should_reject_a_delta_with_trailing_bytes) {
      NetSnapshot from;
      AddSkater(from, 1, 10, 20, 100);
      from.Finish();
      NetSnapshot to;
      AddSkater(to, 1, 10, 21, 100);
      to.Finish();
      uint8_t delta[128] = {};
      int n = NetSnapshotDelta::Create(from, to, delta, sizeof(delta));
      Assert::That(n, Is().GreaterThan(0));

      NetSnapshot rebuilt;
      Assert::That(NetSnapshotDelta::Apply(from, delta, n, rebuilt),
                   Equals(true));
      // Two spare bytes on the end: sender and receiver disagree about the
      // encoding, so the delta cannot be trusted even though it parses.
      delta[n] = 0x00;
      delta[n + 1] = 0x01;
      NetSnapshot padded;
      Assert::That(NetSnapshotDelta::Apply(from, delta, n + 2, padded),
                   Equals(false));
    };
    It(should_reject_a_delta_truncated_mid_item) {
      NetSnapshot from;
      AddSkater(from, 1, 10, 20, 100);
      from.Finish();
      NetSnapshot to;
      AddSkater(to, 1, 11, 21, 101);
      AddSkater(to, 2, 1, 2, 3);
      to.Finish();
      uint8_t delta[128] = {};
      int n = NetSnapshotDelta::Create(from, to, delta, sizeof(delta));
      Assert::That(n, Is().GreaterThan(2));
      for (int cut = 1; cut < n; cut++) {
        NetSnapshot rebuilt;
        Assert::That(NetSnapshotDelta::Apply(from, delta, cut, rebuilt),
                     Equals(false));
      }
    };
  };

  // Keys are (type << 16) | id and AddItem takes a uint16 type, so any type
  // from 0x8000 up rides the wire as a negative varint. Create always emitted
  // those correctly; Apply used to reject them outright, so a game whose type
  // ids came from a hash or a grown enum saw every delta silently fail.
  Describe(HighItemTypes) {
    It(should_round_trip_a_type_with_the_top_bit_set) {
      NetSnapshot from;
      from.Finish();
      NetSnapshot to;
      int32_t data[3] = {7, -8, 9};
      Assert::That(to.AddItem(0x8001, 7, data, 3), Equals(true));
      to.Finish();

      uint8_t delta[128];
      int n = NetSnapshotDelta::Create(from, to, delta, sizeof(delta));
      Assert::That(n, Is().GreaterThan(0));
      NetSnapshot rebuilt;
      Assert::That(NetSnapshotDelta::Apply(from, delta, n, rebuilt),
                   Equals(true));
      Assert::That(rebuilt.Crc(), Equals(to.Crc()));
      const int32_t *out = nullptr;
      int count = 0;
      Assert::That(rebuilt.FindItem(0x8001, 7, out, count), Equals(true));
      Assert::That(count, Equals(3));
      Assert::That(out[1], Equals(-8));
    };
    It(should_round_trip_the_largest_possible_key) {
      NetSnapshot from;
      from.Finish();
      NetSnapshot to;
      int32_t data[1] = {42};
      Assert::That(to.AddItem(0xFFFF, 0xFFFF, data, 1), Equals(true));
      to.Finish();

      uint8_t delta[64];
      int n = NetSnapshotDelta::Create(from, to, delta, sizeof(delta));
      NetSnapshot rebuilt;
      Assert::That(NetSnapshotDelta::Apply(from, delta, n, rebuilt),
                   Equals(true));
      const int32_t *out = nullptr;
      int count = 0;
      Assert::That(rebuilt.FindItem(0xFFFF, 0xFFFF, out, count), Equals(true));
      Assert::That(out[0], Equals(42));
    };
    It(should_diff_and_delete_high_typed_items) {
      NetSnapshot from;
      int32_t base[2] = {10, 20};
      Assert::That(from.AddItem(0xC000, 3, base, 2), Equals(true));
      Assert::That(from.AddItem(0x8000, 0, base, 2), Equals(true));
      from.Finish();

      NetSnapshot to; // one changed, one gone
      int32_t moved[2] = {11, 20};
      Assert::That(to.AddItem(0xC000, 3, moved, 2), Equals(true));
      to.Finish();

      uint8_t delta[128];
      int n = NetSnapshotDelta::Create(from, to, delta, sizeof(delta));
      Assert::That(n, Is().GreaterThan(0));
      NetSnapshot rebuilt;
      Assert::That(NetSnapshotDelta::Apply(from, delta, n, rebuilt),
                   Equals(true));
      Assert::That(rebuilt.NumItems(), Equals(1));
      Assert::That(rebuilt.Crc(), Equals(to.Crc()));
      const int32_t *out = nullptr;
      int count = 0;
      Assert::That(rebuilt.FindItem(0x8000, 0, out, count), Equals(false));
      Assert::That(rebuilt.FindItem(0xC000, 3, out, count), Equals(true));
      Assert::That(out[0], Equals(11));
    };
    It(should_keep_high_and_low_typed_items_in_one_delta) {
      NetSnapshot from;
      AddSkater(from, 1, 10, 20, 100);
      from.Finish();
      NetSnapshot to;
      AddSkater(to, 1, 10, 21, 100);
      int32_t data[2] = {-1, -2};
      Assert::That(to.AddItem(0xABCD, 0x1234, data, 2), Equals(true));
      to.Finish();

      uint8_t delta[256];
      int n = NetSnapshotDelta::Create(from, to, delta, sizeof(delta));
      NetSnapshot rebuilt;
      Assert::That(NetSnapshotDelta::Apply(from, delta, n, rebuilt),
                   Equals(true));
      Assert::That(rebuilt.NumItems(), Equals(2));
      Assert::That(rebuilt.Crc(), Equals(to.Crc()));
    };
  };

  Describe(Cache) {
    It(should_be_empty_when_freshly_constructed) {
      NetSnapshotCache cache; // stack slot — used_/ticks_ must be zeroed
      Assert::That(cache.GetLatestTick(), Equals(-1));
      for (int t = -16; t <= 16; t++)
        Assert::That(cache.Get(t), Equals((NetSnapshot *)nullptr));
    };
    It(should_store_and_retrieve_by_tick) {
      NetSnapshotCache cache;
      NetSnapshot snap;
      AddPuck(snap, 1, 2);
      snap.Finish();
      cache.Store(100, snap);
      Assert::That(cache.Get(100), Is().Not().EqualTo((NetSnapshot *)nullptr));
      Assert::That(cache.Get(101), Equals((NetSnapshot *)nullptr));
      Assert::That(cache.GetLatestTick(), Equals(100));
    };
    It(should_overwrite_the_same_tick) {
      NetSnapshotCache cache;
      NetSnapshot a;
      AddPuck(a, 1, 2);
      a.Finish();
      NetSnapshot b;
      AddPuck(b, 9, 9);
      b.Finish();
      cache.Store(5, a);
      cache.Store(5, b);
      Assert::That(cache.Get(5)->Crc(), Equals(b.Crc()));
    };
    It(should_track_the_latest_tick) {
      NetSnapshotCache cache;
      NetSnapshot snap;
      AddPuck(snap, 1, 2);
      snap.Finish();
      cache.Store(10, snap);
      cache.Store(30, snap);
      cache.Store(20, snap);
      Assert::That(cache.GetLatestTick(), Equals(30));
      cache.Reset();
      Assert::That(cache.GetLatestTick(), Equals(-1));
      Assert::That(cache.Get(30), Equals((NetSnapshot *)nullptr));
    };
  };
};
