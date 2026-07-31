#include <algorithm>

#include "netSnapshot.h"
#include "netVarInt.h"

static uint32_t MakeKey(uint16_t type, uint16_t id) {
  return ((uint32_t)type << 16) | id;
}

void NetSnapshot::Reset() {
  numItems_ = 0;
  dataCount_ = 0;
  finished_ = false;
}

bool NetSnapshot::AddItem(uint16_t type, uint16_t id, const int32_t *data,
                          int count) {
  if (finished_ || count < 0 || dataCount_ + count > kMaxDataInts)
    return false;

  uint32_t key = MakeKey(type, id);
  int index = 0;
  if (FindKey(key, index)) {
    // Replace in place; the count must match or the item would report a
    // stale trailing ints / corrupt the layout.
    int existingCount = (index + 1 < numItems_)
                            ? offsets_[index + 1] - offsets_[index]
                            : dataCount_ - offsets_[index];
    if (count != existingCount)
      return false;
    for (int i = 0; i < count; i++)
      data_[offsets_[index] + i] = data[i];
    return true;
  }

  if (numItems_ >= kMaxItems)
    return false;
  keys_[numItems_] = key;
  offsets_[numItems_] = dataCount_;
  for (int i = 0; i < count; i++)
    data_[dataCount_ + i] = data[i];
  dataCount_ += count;
  numItems_++;
  return true;
}

int NetSnapshot::Finish() {
  if (finished_)
    return dataCount_;
  finished_ = true;

  // Sort an index permutation by key so FindItem can binary search.
  int order_[kMaxItems];
  for (int i = 0; i < numItems_; i++)
    order_[i] = i;
  std::sort(order_, order_ + numItems_,
            [this](int a, int b) { return keys_[a] < keys_[b]; });

  uint32_t sortedKeys_[kMaxItems];
  int sortedOffsets_[kMaxItems];
  for (int i = 0; i < numItems_; i++) {
    sortedKeys_[i] = keys_[order_[i]];
    sortedOffsets_[i] = offsets_[order_[i]];
  }
  for (int i = 0; i < numItems_; i++) {
    keys_[i] = sortedKeys_[i];
    offsets_[i] = sortedOffsets_[i];
  }
  return dataCount_;
}

bool NetSnapshot::FindKey(uint32_t key, int &index) const {
  for (int i = 0; i < numItems_; i++) {
    if (keys_[i] == key) {
      index = i;
      return true;
    }
  }
  return false;
}

bool NetSnapshot::FindItem(uint16_t type, uint16_t id, const int32_t *&data,
                           int &count) const {
  if (!finished_)
    return false;
  uint32_t key = MakeKey(type, id);
  int lo = 0, hi = numItems_ - 1;
  while (lo <= hi) {
    int mid = (lo + hi) / 2;
    if (keys_[mid] < key)
      lo = mid + 1;
    else if (keys_[mid] > key)
      hi = mid - 1;
    else {
      data = data_ + offsets_[mid];
      count = (mid + 1 < numItems_) ? offsets_[mid + 1] - offsets_[mid]
                                    : dataCount_ - offsets_[mid];
      return true;
    }
  }
  return false;
}

bool NetSnapshot::GetItemByIndex(int index, uint16_t &type, uint16_t &id,
                                 const int32_t *&data, int &count) const {
  if (!finished_ || index < 0 || index >= numItems_)
    return false;
  type = (uint16_t)(keys_[index] >> 16);
  id = (uint16_t)(keys_[index] & 0xFFFF);
  data = data_ + offsets_[index];
  count = (index + 1 < numItems_) ? offsets_[index + 1] - offsets_[index]
                                  : dataCount_ - offsets_[index];
  return true;
}

uint32_t NetSnapshot::Crc() const {
  uint32_t crc = 0;
  for (int i = 0; i < numItems_; i++) {
    crc += keys_[i];
    int end = (i + 1 < numItems_) ? offsets_[i + 1] : dataCount_;
    for (int j = offsets_[i]; j < end; j++)
      crc += (uint32_t)data_[j];
  }
  return crc;
}

int NetSnapshotDelta::Create(const NetSnapshot &from, const NetSnapshot &to,
                             uint8_t *dst, int dstSize) {
  int written = 0;
  auto putVarInt = [&](int32_t v) {
    if (written < 0)
      return;
    int n = NetVarIntPack(dst + written, dstSize - written, v);
    written = (n > 0) ? written + n : -1;
  };

  int numDeleted = 0;
  for (int i = 0; i < from.NumItems(); i++) {
    uint16_t t, id;
    const int32_t *d;
    int c;
    from.GetItemByIndex(i, t, id, d, c);
    if (!to.FindItem(t, id, d, c))
      numDeleted++;
  }

  putVarInt(numDeleted);
  for (int i = 0; i < from.NumItems() && written >= 0; i++) {
    uint16_t t, id;
    const int32_t *fd;
    int fc;
    from.GetItemByIndex(i, t, id, fd, fc);
    const int32_t *td;
    int tc;
    if (!to.FindItem(t, id, td, tc))
      putVarInt((int32_t)(((uint32_t)t << 16) | id));
  }

  int numUpdated = 0;
  for (int i = 0; i < to.NumItems(); i++) {
    uint16_t t, id;
    const int32_t *d;
    int c;
    to.GetItemByIndex(i, t, id, d, c);
    const int32_t *fd;
    int fc;
    if (from.FindItem(t, id, fd, fc)) {
      bool changed = false;
      for (int j = 0; j < c && j < fc; j++) {
        if (d[j] != fd[j]) {
          changed = true;
          break;
        }
      }
      if (changed || c != fc)
        numUpdated++;
    } else {
      numUpdated++;
    }
  }

  if (numDeleted == 0 && numUpdated == 0)
    return 0; // nothing changed: send nothing

  putVarInt(numUpdated);
  for (int i = 0; i < to.NumItems() && written >= 0; i++) {
    uint16_t t, id;
    const int32_t *td;
    int tc;
    to.GetItemByIndex(i, t, id, td, tc);
    const int32_t *fd;
    int fc;
    bool inFrom = from.FindItem(t, id, fd, fc);
    if (inFrom && fc == tc) {
      bool same = true;
      for (int j = 0; j < tc; j++) {
        if (td[j] != fd[j]) {
          same = false;
          break;
        }
      }
      if (same)
        continue; // unchanged, omit
    }
    putVarInt((int32_t)(((uint32_t)t << 16) | id));
    putVarInt(tc);
    for (int j = 0; j < tc; j++)
      putVarInt((inFrom && fc == tc) ? td[j] - fd[j]
                                     : td[j]); // diffs only for equal counts
  }

  return written;
}

bool NetSnapshotDelta::Apply(const NetSnapshot &from, const uint8_t *delta,
                             int deltaSize, NetSnapshot &to) {
  to.Reset();
  int pos = 0;
  auto getVarInt = [&](int32_t &out) {
    if (pos < 0)
      return false;
    int consumed = 0;
    if (!NetVarIntUnpack(delta + pos, deltaSize - pos, out, consumed))
      return false;
    pos += consumed;
    return true;
  };

  int32_t numDeleted = 0, numUpdated = 0;
  if (!getVarInt(numDeleted) || numDeleted < 0 || numDeleted > from.NumItems())
    return false;

  // Read the deleted keys once.
  uint32_t deletedKeys[NetSnapshot::kMaxItems];
  for (int k = 0; k < numDeleted; k++) {
    int32_t delKey = 0;
    if (!getVarInt(delKey) || delKey < 0)
      return false;
    deletedKeys[k] = (uint32_t)delKey;
  }

  if (!getVarInt(numUpdated) || numUpdated < 0 ||
      numUpdated > NetSnapshot::kMaxItems)
    return false;

  // The update list starts right after numUpdated; pass 2 rewinds here.
  int updatesStart = pos;

  // Pass 1: read the update list headers (key + count) and skip the values.
  // An item that grows or shrinks cannot replace the base copy in place, so
  // the base copy below skips everything the update list (re)applies.
  uint32_t updateKeys[NetSnapshot::kMaxItems];
  int updateCounts[NetSnapshot::kMaxItems];
  for (int u = 0; u < numUpdated; u++) {
    int32_t key = 0, count = 0;
    if (!getVarInt(key) || key < 0)
      return false;
    if (!getVarInt(count) || count < 0 || count > NetSnapshot::kMaxDataInts)
      return false;
    updateKeys[u] = (uint32_t)key;
    updateCounts[u] = count;
    for (int j = 0; j < count; j++) {
      int32_t v = 0;
      if (!getVarInt(v))
        return false;
    }
  }

  // Copy the base items, skipping deleted keys and updated keys.
  for (int i = 0; i < from.NumItems(); i++) {
    uint16_t t, id;
    const int32_t *d;
    int c;
    from.GetItemByIndex(i, t, id, d, c);
    uint32_t key = ((uint32_t)t << 16) | id;
    bool skip = false;
    for (int k = 0; k < numDeleted; k++) {
      if (deletedKeys[k] == key) {
        skip = true;
        break;
      }
    }
    for (int u = 0; !skip && u < numUpdated; u++) {
      if (updateKeys[u] == key) {
        skip = true;
        break;
      }
    }
    if (!skip && !to.AddItem(t, id, d, c))
      return false;
  }

  // Pass 2: apply updates as fresh items — diffs against the base for
  // equal-count items, absolutes for new or count-changed ones. The key,
  // count, and values are re-read contiguously from the update list.
  pos = updatesStart; // re-read the update list from the wire
  for (int u = 0; u < numUpdated; u++) {
    int32_t key = 0, count = 0;
    if (!getVarInt(key) || key < 0)
      return false;
    if (!getVarInt(count) || count < 0 || count > NetSnapshot::kMaxDataInts)
      return false;
    uint16_t t = (uint16_t)(key >> 16);
    uint16_t id = (uint16_t)(key & 0xFFFF);
    const int32_t *fd;
    int fc;
    bool inFrom = from.FindItem(t, id, fd, fc);
    bool diff = inFrom && fc == count;
    int32_t values[NetSnapshot::kMaxDataInts];
    for (int j = 0; j < count; j++) {
      int32_t v = 0;
      if (!getVarInt(v))
        return false;
      values[j] = diff ? fd[j] + v : v;
    }
    if (!to.AddItem(t, id, values, count))
      return false;
  }

  to.Finish();
  return true;
}

int NetSnapshotDelta::EstimateSize(const NetSnapshot &from,
                                   const NetSnapshot &to) {
  // Conservative: every item deleted or updated, each value 5 bytes.
  return 2 + (from.NumItems() + to.NumItems()) * 11 + to.TotalDataInts() * 5;
}

void NetSnapshotCache::Store(int tick, const NetSnapshot &snap) {
  for (int i = 0; i < kSize; i++) {
    if (used_[i] && ticks_[i] == tick) {
      snaps_[i] = snap;
      return;
    }
  }
  ticks_[nextSlot_] = tick;
  snaps_[nextSlot_] = snap;
  used_[nextSlot_] = true;
  nextSlot_ = (nextSlot_ + 1) % kSize;
  if (tick > latestTick_)
    latestTick_ = tick;
}

const NetSnapshot *NetSnapshotCache::Get(int tick) const {
  for (int i = 0; i < kSize; i++) {
    if (used_[i] && ticks_[i] == tick)
      return &snaps_[i];
  }
  return nullptr;
}

void NetSnapshotCache::Reset() {
  for (int i = 0; i < kSize; i++)
    used_[i] = false;
  nextSlot_ = 0;
  latestTick_ = -1;
}
