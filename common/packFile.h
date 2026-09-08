#pragma once

#include <cstdint>
#include <cstring>
#include <istream>
#include <ostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace storm {

// PackFile is the engine's one-file asset container: a build tool writes
// every game asset into a single opaque file, and the game reads blobs out of
// it by name. There is no folder for players to browse or edit and no
// standard tool that opens it. No compression (pixel-exact bytes in, byte-
// exact bytes out) and no encryption (this is tamper/casual-browse
// prevention, not DRM).
//
// Header-only, SDL-free and exception-free: a game opens the pack with
// std::ifstream (or any std::istream), and hands a Read() buffer to
// SDL_RWFromMem / IMG_Load_RW on the game side. Every failure is a bool --
// the Switch build compiles with -fno-exceptions.
//
// File format v1, all integers little-endian:
//
//   bytes 0..3   magic "SPAK"
//   bytes 4..7   version (u32, currently 1)
//   bytes 8..11  entry count (u32)
//   per entry:   name length (u16), name bytes (no NUL),
//                blob offset from file start (u64), blob size (u64)
//   then:        the blobs themselves, back to back
//
// The reader validates magic, version and stream state and refuses anything
// else; a truncated or corrupt pack fails Open, it never yields wrong bytes.
// Every index entry is bounds-checked against the file length at Open time,
// so a corrupt size can waste no memory later -- Read's resize is of a size
// the file actually contains.

// Writes a pack. Entries are held in memory (this is build-tool side), so a
// game process only ever uses this to assemble small runtime packs.
struct PackWriter {
  // Shared with PackReader so the two cannot drift; reader and writer are
  // one schema (CODING.md tenet 5).
  static constexpr uint32_t kMagic = 0x4B415053u; // "SPAK" little-endian
  static constexpr uint32_t kVersion = 1;

  // Adds a copy of the blob under `name`. Rejects duplicates, empty names
  // and names the u16 length field cannot express.
  bool Add(const std::string &name, const std::vector<uint8_t> &data) {
    if (name.empty() || name.size() > 0xFFFFu) {
      return false;
    }
    return entries.emplace(name, data).second;
  }

  std::size_t Count() const { return entries.size(); }

  // Serialises the pack. Offsets are computed here, so Add order does not
  // matter and nothing in the index can go stale.
  bool Save(std::ostream &out) const {
    if (entries.size() > 0xFFFFFFFFu) {
      return false; // the u32 count field cannot express this many entries
    }
    PutU32(out, kMagic);
    PutU32(out, kVersion);
    PutU32(out, static_cast<uint32_t>(entries.size()));

    // Index first: each entry is 2 + nameLen + 8 + 8 bytes.
    uint64_t offset = 12;
    for (const auto &entry : entries) {
      offset += 2 + entry.first.size() + 16;
    }
    for (const auto &entry : entries) {
      const std::string &name = entry.first;
      PutU16(out, static_cast<uint16_t>(name.size()));
      out.write(name.data(), static_cast<std::streamsize>(name.size()));
      PutU64(out, offset);
      PutU64(out, static_cast<uint64_t>(entry.second.size()));
      offset += entry.second.size();
    }

    for (const auto &entry : entries) {
      out.write(reinterpret_cast<const char *>(entry.second.data()),
                static_cast<std::streamsize>(entry.second.size()));
    }
    return static_cast<bool>(out);
  }

private:
  static void PutU16(std::ostream &out, uint16_t v) {
    const uint8_t b[2] = {static_cast<uint8_t>(v),
                          static_cast<uint8_t>(v >> 8)};
    out.write(reinterpret_cast<const char *>(b), 2);
  }

  static void PutU32(std::ostream &out, uint32_t v) {
    const uint8_t b[4] = {static_cast<uint8_t>(v), static_cast<uint8_t>(v >> 8),
                          static_cast<uint8_t>(v >> 16),
                          static_cast<uint8_t>(v >> 24)};
    out.write(reinterpret_cast<const char *>(b), 4);
  }

  static void PutU64(std::ostream &out, uint64_t v) {
    const uint8_t b[8] = {
        static_cast<uint8_t>(v),       static_cast<uint8_t>(v >> 8),
        static_cast<uint8_t>(v >> 16), static_cast<uint8_t>(v >> 24),
        static_cast<uint8_t>(v >> 32), static_cast<uint8_t>(v >> 40),
        static_cast<uint8_t>(v >> 48), static_cast<uint8_t>(v >> 56)};
    out.write(reinterpret_cast<const char *>(b), 8);
  }

  std::unordered_map<std::string, std::vector<uint8_t>> entries;
};

// Reads a pack. Open() parses the index once; Read() seeks to the blob and
// fills a caller-owned buffer. The reader keeps a pointer to the stream, so
// the stream must outlive the PackReader, and it may be positioned anywhere
// before Open and between Reads -- every operation seeks first.
struct PackReader {
  // Parses the index from `in`. False for bad magic, an unsupported version,
  // a corrupt or truncated index, or an empty entry name. On false the
  // reader is empty and safe to reuse against another stream.
  bool Open(std::istream &in) {
    index_.clear();
    stream_ = nullptr;
    in.clear(); // a reused stream may carry a stale fail/eof bit
    if (!in) {
      return false;
    }

    // Read needs to seek, so the stream must be measureable; a length below
    // the 12-byte header means tellg() failed or the file is truncated.
    in.seekg(0, std::ios::end);
    const std::streamoff endPos = in.tellg();
    in.seekg(0);
    if (!in || endPos < 12) {
      return false;
    }
    const uint64_t fileLen = static_cast<uint64_t>(endPos);

    uint32_t magic = 0;
    uint32_t version = 0;
    uint32_t count = 0;
    GetU32(in, magic);
    GetU32(in, version);
    GetU32(in, count);
    if (!in || magic != PackWriter::kMagic || version != PackWriter::kVersion) {
      return false;
    }
    if (count == 0) {
      stream_ = &in; // a valid, empty pack
      return true;
    }

    for (uint32_t i = 0; i < count; ++i) {
      uint16_t nameLen = 0;
      GetU16(in, nameLen);
      if (!in || nameLen == 0) {
        return false;
      }
      std::string name(nameLen, '\0');
      in.read(&name[0], static_cast<std::streamsize>(nameLen));
      uint64_t offset = 0;
      uint64_t size = 0;
      GetU64(in, offset);
      GetU64(in, size);
      if (!in) {
        return false;
      }
      // Bounds-check by subtraction, not offset + size > fileLen: the
      // addition could overflow u64 and pass a hostile entry. A rejected
      // entry here is one Read can never read past the file.
      if (offset > fileLen || size > fileLen - offset) {
        return false;
      }
      index_.emplace(name, Entry{offset, size}); // first wins on duplicates
    }

    stream_ = &in;
    return true;
  }

  std::size_t Count() const { return index_.size(); }

  bool Has(const std::string &name) const {
    return index_.find(name) != index_.end();
  }

  // Blob size in bytes without reading it. False when the name is unknown.
  bool SizeOf(const std::string &name, uint64_t &size) const {
    const auto it = index_.find(name);
    if (it == index_.end()) {
      return false;
    }
    size = it->second.size;
    return true;
  }

  // Reads the blob into `out` (resized to the exact blob size). False when
  // the name is unknown or the stream cannot deliver the bytes; `out` is
  // left alone on an unknown name, and on a stream failure it is already
  // resized (zero-filled) with unspecified contents.
  bool Read(const std::string &name, std::vector<uint8_t> &out) const {
    const auto it = index_.find(name);
    if (it == index_.end() || stream_ == nullptr) {
      return false;
    }
    out.resize(static_cast<std::size_t>(it->second.size));
    if (out.empty()) {
      return true;
    }
    stream_->clear();
    stream_->seekg(static_cast<std::streamoff>(it->second.offset));
    stream_->read(reinterpret_cast<char *>(out.data()),
                  static_cast<std::streamsize>(out.size()));
    return static_cast<bool>(*stream_);
  }

private:
  static void GetU16(std::istream &in, uint16_t &v) {
    uint8_t b[2] = {0, 0};
    in.read(reinterpret_cast<char *>(b), 2);
    if (in) {
      v = static_cast<uint16_t>(b[0]) |
          static_cast<uint16_t>(static_cast<uint16_t>(b[1]) << 8);
    }
  }

  static void GetU32(std::istream &in, uint32_t &v) {
    uint8_t b[4] = {0, 0, 0, 0};
    in.read(reinterpret_cast<char *>(b), 4);
    if (in) {
      v = static_cast<uint32_t>(b[0]) | (static_cast<uint32_t>(b[1]) << 8) |
          (static_cast<uint32_t>(b[2]) << 16) |
          (static_cast<uint32_t>(b[3]) << 24);
    }
  }

  static void GetU64(std::istream &in, uint64_t &v) {
    uint8_t b[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    in.read(reinterpret_cast<char *>(b), 8);
    if (in) {
      v = static_cast<uint64_t>(b[0]) | (static_cast<uint64_t>(b[1]) << 8) |
          (static_cast<uint64_t>(b[2]) << 16) |
          (static_cast<uint64_t>(b[3]) << 24) |
          (static_cast<uint64_t>(b[4]) << 32) |
          (static_cast<uint64_t>(b[5]) << 40) |
          (static_cast<uint64_t>(b[6]) << 48) |
          (static_cast<uint64_t>(b[7]) << 56);
    }
  }

  struct Entry {
    uint64_t offset;
    uint64_t size;
  };

  std::unordered_map<std::string, Entry> index_;
  std::istream *stream_ = nullptr;
};

} // namespace storm
