#include <igloo/igloo_alt.h>

#include <cstdint>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

#include "../common/packFile.h"

using namespace igloo;
using namespace storm;

// PackFile is the one-file asset container: a build tool writes a pack with
// PackWriter, the game reads it back with PackReader. Both are header-only,
// SDL-free and exception-free (the Switch build compiles with
// -fno-exceptions), so every failure is a bool and every spec here is a
// round-trip through std::stringstream -- no disk, no SDL, no window.
Describe(PackFileSpec) {

  It(should_round_trip_a_single_entry) {
    PackWriter writer;
    const std::vector<uint8_t> data{1, 2, 3, 4, 5};
    Assert::That(writer.Add("sprites/player.png", data), Equals(true));

    std::ostringstream out;
    Assert::That(writer.Save(out), Equals(true));

    std::istringstream in(out.str());
    PackReader reader;
    Assert::That(reader.Open(in), Equals(true));
    Assert::That(reader.Has("sprites/player.png"), Equals(true));

    std::vector<uint8_t> read;
    Assert::That(reader.Read("sprites/player.png", read), Equals(true));
    Assert::That(read, Equals(data));
  };

  It(should_round_trip_many_entries_with_binary_data) {
    PackWriter writer;
    // 0x00 and 0xFF bytes, plus a >64KB blob, so no test can pass on
    // accidental truncation or sign-extension of a size field.
    std::vector<uint8_t> a{0x00, 0xFF, 0x00, 0x7F};
    std::vector<uint8_t> big(100000);
    for (std::size_t i = 0; i < big.size(); ++i) {
      big[i] = static_cast<uint8_t>(i * 7);
    }
    std::vector<uint8_t> empty;
    Assert::That(writer.Add("a.bin", a), Equals(true));
    Assert::That(writer.Add("big.bin", big), Equals(true));
    Assert::That(writer.Add("empty.bin", empty), Equals(true));

    std::ostringstream out;
    Assert::That(writer.Save(out), Equals(true));

    std::istringstream in(out.str());
    PackReader reader;
    Assert::That(reader.Open(in), Equals(true));
    Assert::That(reader.Count(), Equals(3u));

    std::vector<uint8_t> read;
    Assert::That(reader.Read("a.bin", read), Equals(true));
    Assert::That(read, Equals(a));
    Assert::That(reader.Read("big.bin", read), Equals(true));
    Assert::That(read, Equals(big));
    Assert::That(reader.Read("empty.bin", read), Equals(true));
    Assert::That(read, Equals(empty));
  };

  It(should_round_trip_a_long_name) {
    PackWriter writer;
    const std::string longName(255, 'x');
    const std::vector<uint8_t> data{9, 9};
    Assert::That(writer.Add(longName, data), Equals(true));

    std::ostringstream out;
    Assert::That(writer.Save(out), Equals(true));

    std::istringstream in(out.str());
    PackReader reader;
    Assert::That(reader.Open(in), Equals(true));
    Assert::That(reader.Has(longName), Equals(true));
    std::vector<uint8_t> read;
    Assert::That(reader.Read(longName, read), Equals(true));
    Assert::That(read, Equals(data));
  };

  It(should_reject_a_duplicate_name) {
    PackWriter writer;
    const std::vector<uint8_t> first{1};
    const std::vector<uint8_t> second{2, 2};
    Assert::That(writer.Add("dup.bin", first), Equals(true));
    Assert::That(writer.Add("dup.bin", second), Equals(false));
    Assert::That(writer.Count(), Equals(1u));
  };

  It(should_reject_an_empty_name) {
    PackWriter writer;
    const std::vector<uint8_t> data{1};
    Assert::That(writer.Add("", data), Equals(false));
  };

  It(should_reject_a_name_longer_than_the_format_allows) {
    PackWriter writer;
    const std::string tooLong(65536, 'y');
    const std::vector<uint8_t> data{1};
    Assert::That(writer.Add(tooLong, data), Equals(false));
  };

  It(should_fail_to_open_a_file_with_bad_magic) {
    std::istringstream in(std::string("NOPE........"));
    PackReader reader;
    Assert::That(reader.Open(in), Equals(false));
    Assert::That(reader.Count(), Equals(0u));
  };

  It(should_fail_to_open_a_truncated_file) {
    std::ostringstream out;
    PackWriter writer;
    Assert::That(writer.Add("x.bin", std::vector<uint8_t>{1, 2, 3}),
                 Equals(true));
    Assert::That(writer.Save(out), Equals(true));

    const std::string full = out.str();
    std::istringstream in(full.substr(0, full.size() / 2));
    PackReader reader;
    Assert::That(reader.Open(in), Equals(false));
  };

  It(should_fail_to_open_an_unsupported_version) {
    // A valid v1 pack with the version field patched to 99: the reader
    // refuses it rather than guessing.
    std::ostringstream out;
    PackWriter writer;
    Assert::That(writer.Add("x.bin", std::vector<uint8_t>{1}), Equals(true));
    Assert::That(writer.Save(out), Equals(true));
    std::string bytes = out.str();
    bytes[4] = 99;

    std::istringstream in(bytes);
    PackReader reader;
    Assert::That(reader.Open(in), Equals(false));
  };

  It(should_open_an_empty_pack) {
    std::ostringstream out;
    PackWriter writer;
    Assert::That(writer.Save(out), Equals(true));

    std::istringstream in(out.str());
    PackReader reader;
    Assert::That(reader.Open(in), Equals(true));
    Assert::That(reader.Count(), Equals(0u));
    Assert::That(reader.Has("anything"), Equals(false));
  };

  It(should_fail_to_read_a_missing_name_and_leave_out_alone) {
    std::ostringstream out;
    PackWriter writer;
    Assert::That(writer.Add("real.bin", std::vector<uint8_t>{1}), Equals(true));
    Assert::That(writer.Save(out), Equals(true));

    std::istringstream in(out.str());
    PackReader reader;
    Assert::That(reader.Open(in), Equals(true));

    std::vector<uint8_t> read{42};
    Assert::That(reader.Read("missing.bin", read), Equals(false));
    Assert::That(read.size(), Equals(1u)); // untouched
    Assert::That(read[0], Equals(42));
  };

  It(should_report_sizes_without_reading_the_blob) {
    std::ostringstream out;
    PackWriter writer;
    std::vector<uint8_t> data(1234, 7);
    Assert::That(writer.Add("sized.bin", data), Equals(true));
    Assert::That(writer.Save(out), Equals(true));

    std::istringstream in(out.str());
    PackReader reader;
    Assert::That(reader.Open(in), Equals(true));
    uint64_t size = 0;
    Assert::That(reader.SizeOf("sized.bin", size), Equals(true));
    Assert::That(size, Equals(1234u));
  };

  It(should_read_after_the_stream_was_left_mid_file) {
    // Open must not depend on the stream's initial position, and Read must
    // not depend on where a previous Read left it.
    std::ostringstream out;
    PackWriter writer;
    Assert::That(writer.Add("one.bin", std::vector<uint8_t>{1}), Equals(true));
    Assert::That(writer.Add("two.bin", std::vector<uint8_t>{2, 2}),
                 Equals(true));
    Assert::That(writer.Save(out), Equals(true));

    std::istringstream in(out.str());
    in.seekg(5); // deliberately mid-file
    PackReader reader;
    Assert::That(reader.Open(in), Equals(true));

    std::vector<uint8_t> read;
    Assert::That(reader.Read("two.bin", read), Equals(true));
    Assert::That(read, Equals(std::vector<uint8_t>{2, 2}));
    Assert::That(reader.Read("one.bin", read), Equals(true));
    Assert::That(read, Equals(std::vector<uint8_t>{1}));
  };

  // ── Hand-built bytes for corruption cases no writer can produce ──

  static void PutU32(std::vector<uint8_t> & b, uint32_t v) {
    b.push_back(static_cast<uint8_t>(v));
    b.push_back(static_cast<uint8_t>(v >> 8));
    b.push_back(static_cast<uint8_t>(v >> 16));
    b.push_back(static_cast<uint8_t>(v >> 24));
  }

  static void PutU64(std::vector<uint8_t> & b, uint64_t v) {
    for (int i = 0; i < 8; ++i) {
      b.push_back(static_cast<uint8_t>(v >> (8 * i)));
    }
  }

  // Header + `count`-entry index with no blobs; entries appended by the
  // caller as PutU16(nameLen) + name + PutU64(offset) + PutU64(size).
  static std::vector<uint8_t> HeaderWithCount(uint32_t count) {
    std::vector<uint8_t> b;
    PutU32(b, PackWriter::kMagic);
    PutU32(b, PackWriter::kVersion);
    PutU32(b, count);
    return b;
  }

  It(should_reject_a_header_truncated_before_the_count) {
    // "SPAK" + version 1 and nothing else: 8 bytes of a 12-byte header.
    std::vector<uint8_t> b = HeaderWithCount(1);
    b.resize(8);
    std::istringstream in(std::string(b.begin(), b.end()));
    PackReader reader;
    Assert::That(reader.Open(in), Equals(false));
  };

  It(should_reject_an_entry_whose_offset_lies_past_the_file) {
    std::vector<uint8_t> b = HeaderWithCount(1);
    b.push_back(1);
    b.push_back(0); // u16 name length
    b.push_back('a');
    PutU64(b, 0x8000000000000000ull); // offset past any real file
    PutU64(b, 1);                     // size
    std::istringstream in(std::string(b.begin(), b.end()));
    PackReader reader;
    Assert::That(reader.Open(in), Equals(false));
  };

  It(should_reject_an_entry_whose_size_overruns_the_file) {
    // offset+size would overflow u64: the bounds check must survive that.
    std::vector<uint8_t> b = HeaderWithCount(1);
    b.push_back(1);
    b.push_back(0); // u16 name length
    b.push_back('a');
    PutU64(b, 0xFFFFFFFFFFFFFFFFull); // size
    PutU64(b, 1);                     // offset (file itself is 31 bytes)
    std::istringstream in(std::string(b.begin(), b.end()));
    PackReader reader;
    Assert::That(reader.Open(in), Equals(false));
    Assert::That(reader.Count(), Equals(0u));
  };

  It(should_keep_the_first_of_duplicate_names_in_a_hand_built_pack) {
    // Two entries named 'a': the first wins. Blobs at 50: {7} and {8, 9}.
    std::vector<uint8_t> b = HeaderWithCount(2);
    b.push_back(1);
    b.push_back(0); // u16 name length
    b.push_back('a');
    PutU64(b, 50);
    PutU64(b, 1);
    b.push_back(1);
    b.push_back(0); // u16 name length
    b.push_back('a');
    PutU64(b, 51);
    PutU64(b, 2);
    b.push_back(7);
    b.push_back(8);
    b.push_back(9);
    std::istringstream in(std::string(b.begin(), b.end()));
    PackReader reader;
    Assert::That(reader.Open(in), Equals(true));
    std::vector<uint8_t> read;
    Assert::That(reader.Read("a", read), Equals(true));
    Assert::That(read, Equals(std::vector<uint8_t>{7}));
  };

  It(should_open_a_hand_built_pack_with_an_in_range_zero_size_entry) {
    std::vector<uint8_t> b = HeaderWithCount(1);
    b.push_back(1);
    b.push_back(0); // u16 name length
    b.push_back('a');
    PutU64(b, 31); // first byte after the index
    PutU64(b, 0);  // empty blob
    std::istringstream in(std::string(b.begin(), b.end()));
    PackReader reader;
    Assert::That(reader.Open(in), Equals(true));
    std::vector<uint8_t> read{1};
    Assert::That(reader.Read("a", read), Equals(true));
    Assert::That(read.empty(), Equals(true));
  };

  It(should_reject_a_stream_in_a_failed_state_only_until_cleared) {
    // Open clears a stale fail bit rather than failing forever on a reused
    // stream -- mirroring what Read already does.
    std::ostringstream out;
    PackWriter writer;
    Assert::That(writer.Add("x.bin", std::vector<uint8_t>{1}), Equals(true));
    Assert::That(writer.Save(out), Equals(true));

    std::istringstream in(out.str());
    in.setstate(std::ios::failbit);
    PackReader reader;
    Assert::That(reader.Open(in), Equals(true));
    std::vector<uint8_t> read;
    Assert::That(reader.Read("x.bin", read), Equals(true));
    Assert::That(read, Equals(std::vector<uint8_t>{1}));
  };

  It(should_fail_save_when_the_output_stream_fails) {
    PackWriter writer;
    Assert::That(writer.Add("x.bin", std::vector<uint8_t>{1}), Equals(true));
    std::ostringstream bad;
    bad.setstate(std::ios::badbit);
    Assert::That(writer.Save(bad), Equals(false));
  };

  It(should_answer_safely_on_a_reader_that_never_opened) {
    PackReader reader;
    Assert::That(reader.Count(), Equals(0u));
    Assert::That(reader.Has("anything"), Equals(false));
    uint64_t size = 5;
    Assert::That(reader.SizeOf("anything", size), Equals(false));
    std::vector<uint8_t> read{1};
    Assert::That(reader.Read("anything", read), Equals(false));
    Assert::That(read.size(), Equals(1u)); // untouched
  };

  It(should_report_sizeof_false_for_an_unknown_name) {
    std::ostringstream out;
    PackWriter writer;
    Assert::That(writer.Add("real.bin", std::vector<uint8_t>{1}), Equals(true));
    Assert::That(writer.Save(out), Equals(true));

    std::istringstream in(out.str());
    PackReader reader;
    Assert::That(reader.Open(in), Equals(true));
    uint64_t size = 99;
    Assert::That(reader.SizeOf("missing.bin", size), Equals(false));
    Assert::That(size, Equals(99u)); // untouched
  };

  It(should_reuse_the_reader_against_another_stream) {
    std::ostringstream out1;
    PackWriter w1;
    Assert::That(w1.Add("first.bin", std::vector<uint8_t>{1}), Equals(true));
    Assert::That(w1.Save(out1), Equals(true));
    std::ostringstream out2;
    PackWriter w2;
    Assert::That(w2.Add("second.bin", std::vector<uint8_t>{2, 2}),
                 Equals(true));
    Assert::That(w2.Save(out2), Equals(true));

    std::istringstream in1(out1.str());
    PackReader reader;
    Assert::That(reader.Open(in1), Equals(true));
    std::istringstream in2(out2.str());
    Assert::That(reader.Open(in2), Equals(true));
    Assert::That(reader.Count(), Equals(1u));
    Assert::That(reader.Has("first.bin"), Equals(false)); // old index gone
    std::vector<uint8_t> read;
    Assert::That(reader.Read("second.bin", read), Equals(true));
    Assert::That(read, Equals(std::vector<uint8_t>{2, 2}));
  };
};
