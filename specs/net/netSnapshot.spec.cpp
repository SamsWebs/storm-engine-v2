#include <cstring>

#include <igloo/igloo_alt.h>

#include "../../common/net/netSnapshot.h"

using namespace igloo;

namespace {
    void AddSkater(NetSnapshot &snap, int id, int x, int y, int energy) {
        int32_t data[3] = {x, y, energy};
        Assert::That(snap.AddItem(1, (uint16_t)id, data, 3), Equals(true));
    }
    void AddPuck(NetSnapshot &snap, int x, int y) {
        int32_t data[2] = {x, y};
        Assert::That(snap.AddItem(2, 0, data, 2), Equals(true));
    }
}

Describe(NetSnapshotSpec) {

    Describe(Builder) {
        It(should_sort_items_by_key_on_finish) {
            NetSnapshot snap;
            AddPuck(snap, 10, 20);        // key (2<<16)
            AddSkater(snap, 5, 1, 2, 3);  // key (1<<16)|5
            AddSkater(snap, 1, 4, 5, 6);  // key (1<<16)|1
            snap.Finish();
            Assert::That(snap.NumItems(), Equals(3));
            uint16_t type = 0, id = 0;
            const int32_t *data = nullptr;
            int count = 0;
            Assert::That(snap.GetItemByIndex(0, type, id, data, count), Equals(true));
            Assert::That(type, Equals(1)); // skater id 1 sorts before id 5
            Assert::That(id, Equals(1));
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
            Assert::That(NetSnapshotDelta::Apply(from, delta, n, rebuilt), Equals(true));
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
            Assert::That(NetSnapshotDelta::Apply(from, delta, n, rebuilt), Equals(true));
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
            Assert::That(NetSnapshotDelta::Apply(from, delta, n, rebuilt), Equals(true));
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
            Assert::That(NetSnapshotDelta::Create(from, to, delta, sizeof(delta)), Equals(0));
        };
        It(should_reject_malformed_deltas) {
            NetSnapshot from;
            AddSkater(from, 1, 1, 1, 1);
            from.Finish();
            NetSnapshot out;
            uint8_t garbage[] = {0x7F, 0x7F, 0x7F, 0x7F, 0x7F}; // huge counts
            Assert::That(NetSnapshotDelta::Apply(from, garbage, sizeof(garbage), out), Equals(false));
        };
        It(should_apply_a_mixed_delta) {
            NetSnapshot from;
            AddSkater(from, 1, 10, 20, 100);
            AddSkater(from, 2, 50, 60, 70);
            from.Finish();
            NetSnapshot to;
            AddSkater(to, 1, 10, 21, 100);  // changed
            AddSkater(to, 2, 50, 60, 70);   // unchanged
            AddSkater(to, 3, 99, 98, 97);   // new
            to.Finish();
            uint8_t delta[256];
            int n = NetSnapshotDelta::Create(from, to, delta, sizeof(delta));
            Assert::That(n, Is().GreaterThan(0));
            NetSnapshot rebuilt;
            Assert::That(NetSnapshotDelta::Apply(from, delta, n, rebuilt), Equals(true));
            Assert::That(rebuilt.Crc(), Equals(to.Crc()));
        };
        It(should_estimate_size_bounds) {
            NetSnapshot from;
            AddSkater(from, 1, 10, 20, 100);
            from.Finish();
            NetSnapshot to;
            AddSkater(to, 1, 10, 21, 100);
            AddSkater(to, 2, 50, 60, 70);
            to.Finish();
            uint8_t delta[NetSnapshotDelta::EstimateSize(from, to)];
            int n = NetSnapshotDelta::Create(from, to, delta, sizeof(delta));
            Assert::That(n, Is().GreaterThanOrEqualTo(0));
        };
    };

    Describe(Cache) {
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
