#pragma once

#include <cstdint>

#include "netTypes.h"

// ── Game state snapshots (SDL-free) ─────────────────────────────────────────
// A snapshot is a flat set of typed items: each has a type, an id, and an
// int32 payload. The host builds one per game tick (per client), sends deltas
// against what each client last acked; clients apply deltas against their
// stored base snapshots so they can predict. Same idea as Teeworlds'
// CSnapshot/CSnapshotDelta, with a varint-packed delta format:
//
//   [varint numDeleted] [numDeleted x varint key]
//   [varint numUpdated]
//   [numUpdated x: varint key, varint count, count x varint value]
//
// Values are diffs vs the base snapshot for items that exist in both, absolute
// values for new items. Keys are (type << 16) | id.

class NetSnapshot {
public:
    static constexpr int kMaxItems = 256;
    static constexpr int kMaxDataInts = 2048;

    void Reset();
    bool AddItem(uint16_t type, uint16_t id, const int32_t *data, int count); // replaces on duplicate key
    int Finish();           // sorts by key; call before FindItem/GetItemByIndex
    bool IsFinished() const { return finished_; }
    int NumItems() const { return numItems_; }
    int TotalDataInts() const { return dataCount_; }
    bool FindItem(uint16_t type, uint16_t id, const int32_t *&data, int &count) const;
    bool GetItemByIndex(int index, uint16_t &type, uint16_t &id,
                        const int32_t *&data, int &count) const;
    uint32_t Crc() const;

private:
    bool FindKey(uint32_t key, int &index) const;
    uint32_t keys_[kMaxItems];
    int offsets_[kMaxItems];
    int32_t data_[kMaxDataInts];
    int numItems_ = 0;
    int dataCount_ = 0;
    bool finished_ = false;
};

class NetSnapshotDelta {
public:
    // Encodes the difference from -> to. Returns bytes written (0 = no
    // changes, -1 = buffer too small). Apply() rebuilds the target from a base
    // snapshot and a delta produced by Create() against that same base.
    static int Create(const NetSnapshot &from, const NetSnapshot &to,
                      uint8_t *dst, int dstSize);
    static bool Apply(const NetSnapshot &from, const uint8_t *delta, int deltaSize,
                      NetSnapshot &to);
    // Upper bound on the encoded size of a delta between these snapshots.
    static int EstimateSize(const NetSnapshot &from, const NetSnapshot &to);
};

// ── Client-side snapshot cache ──
// Small ring of recent snapshots keyed by game tick, for prediction and as
// delta bases. Store() overwrites an existing tick.

class NetSnapshotCache {
public:
    static constexpr int kSize = 16;

    void Store(int tick, const NetSnapshot &snap);
    const NetSnapshot *Get(int tick) const;
    int GetLatestTick() const { return latestTick_; }
    void Reset();

private:
    NetSnapshot snaps_[kSize];
    int ticks_[kSize];
    bool used_[kSize];
    int nextSlot_ = 0;
    int latestTick_ = -1;
};
