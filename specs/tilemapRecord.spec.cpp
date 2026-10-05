#include "../common/tilemapFormat.h"
#include <igloo/igloo_alt.h>

#include <sstream>

using namespace igloo;
using namespace storm;

namespace {

TileRecord FullRecord() {
  TileRecord r;
  r.group = "tiles";
  r.assetId = "sheet";
  r.tileWidth = 32;
  r.tileHeight = 32;
  r.srcRectX = 0;
  r.srcRectY = 64;
  r.zIndex = 3;
  r.worldX = 96.0f;
  r.worldY = 128.0f;
  r.scaleX = 2.0f;
  r.scaleY = 2.0f;
  r.collider = true;
  r.colliderWidth = 30;
  r.colliderHeight = 30;
  r.colliderOffsetX = 1.0f;
  r.colliderOffsetY = 1.0f;
  r.animated = true;
  r.numFrames = 4;
  r.frameSpeed = 8;
  r.vertical = false;
  r.looped = true;
  r.frameOffset = 2;
  return r;
}

// The trailing newline matters: the writer emits one record per line, and a
// reader must not mistake the next record's group for the last record's
// animation flag.
std::string Line(const std::string &body) { return body + "\n"; }

} // namespace

// P39: the 22-field .map record was parsed by hand, twice -- once in
// common/tilemapLoader.cpp and once in editor/src/utilities/FileLoader.cpp.
// This is the one parser, header-only and spec-reachable, so the editor's copy
// stops being a second implementation that can silently disagree.
//
// The case that matters most is the OMITTED animation flag. The writer always
// emits it, so the bug only appears in a hand-edited file or one written by
// something else -- and the unfixed reading of it loses the WHOLE map, not one
// record. That is the defect these specs pin.
Describe(TileRecordFormatSpec){

    It(should_read_a_full_record_with_collider_and_animation){
        std::istringstream in(
            "tiles sheet 32 32 0 64 3 96 128 2 2 1 30 30 1 1 1 4 8 0 1 2\n");

TileRecordReader reader(in);
TileRecord r;
const bool got = reader.Read(&r);

Assert::That(got, Equals(true));
Assert::That(reader.Truncated(), Equals(false));
Assert::That(r.group, Equals("tiles"));
Assert::That(r.assetId, Equals("sheet"));
Assert::That(r.tileWidth, Equals(32));
Assert::That(r.tileHeight, Equals(32));
Assert::That(r.srcRectX, Equals(0));
Assert::That(r.srcRectY, Equals(64));
Assert::That(r.zIndex, Equals(3));
Assert::That(r.worldX, Equals(96.0f));
Assert::That(r.worldY, Equals(128.0f));
Assert::That(r.scaleX, Equals(2.0f));
Assert::That(r.scaleY, Equals(2.0f));
Assert::That(r.collider, Equals(true));
Assert::That(r.colliderWidth, Equals(30));
Assert::That(r.colliderHeight, Equals(30));
Assert::That(r.colliderOffsetX, Equals(1.0f));
Assert::That(r.colliderOffsetY, Equals(1.0f));
Assert::That(r.animated, Equals(true));
Assert::That(r.numFrames, Equals(4));
Assert::That(r.frameSpeed, Equals(8));
Assert::That(r.vertical, Equals(false));
Assert::That(r.looped, Equals(true));
Assert::That(r.frameOffset, Equals(2));
}

It(should_read_a_record_with_no_collider) {
  std::istringstream in("tiles sheet 32 32 0 0 0 32 32 1 1 0 0\n");

  TileRecordReader reader(in);
  TileRecord r;

  Assert::That(reader.Read(&r), Equals(true));
  Assert::That(r.collider, Equals(false));
  Assert::That(r.animated, Equals(false));
  Assert::That(r.worldX, Equals(32.0f));
}

// ── the load-bearing case ────────────────────────────────────────────
//
// An int extraction that hits the next record's group token and one that
// hits EOF both set failbit, so "this record omitted its flag" and "the
// file ended" are indistinguishable to `>>`. The only available test was
// eof(), which fires on the LAST record and nowhere else -- so a flag
// omitted anywhere else read as a truncated file and the loader returned an
// EMPTY MAP. One hand-edit cost a whole level.

It(should_not_lose_the_map_when_a_middle_record_omits_its_animation_flag) {
  // Record 1 stops after the collider flag: NO animated token at all, so the
  // next thing in the stream is the next record's group name.
  std::istringstream in("tiles sheet 32 32 0 0 0 0 0 1 1 0\n"
                        "tiles sheet 32 32 0 0 0 32 32 1 1 0 1 4 8 1 1 0\n");

  TileRecordReader reader(in);
  TileRecord first;
  TileRecord second;

  Assert::That(reader.Read(&first), Equals(true));
  Assert::That(first.animated, Equals(false));
  Assert::That(first.worldX, Equals(0.0f));
  Assert::That(reader.Truncated(), Equals(false));

  // The second record must still be there. This is the assertion that fails
  // without the pushback: the reader consumed "tiles" as the flag, set
  // failbit, and the whole map ended there.
  Assert::That(reader.Read(&second), Equals(true));
  Assert::That(second.worldX, Equals(32.0f));
  Assert::That(second.animated, Equals(true));
}

It(should_accept_a_final_record_that_legitimately_omits_its_flag) {
  std::istringstream in(
      "tiles sheet 32 32 0 0 0 0 0 1 1 0 1 4 8 1 1 0\n"
      "tiles sheet 32 32 0 0 0 32 32 1 1 0\n"); // no flag, file then ends

  TileRecordReader reader(in);
  TileRecord first;
  TileRecord second;

  Assert::That(reader.Read(&first), Equals(true));
  Assert::That(first.animated, Equals(true));
  Assert::That(reader.Read(&second), Equals(true));
  Assert::That(second.animated, Equals(false));
  Assert::That(reader.Truncated(), Equals(false));
  Assert::That(reader.Read(&second), Equals(false));
}

It(should_report_a_truncated_header_rather_than_ending_silently) {
  std::istringstream in("tiles sheet 32 32 0 0 0 32\n"); // cut mid-header

  TileRecordReader reader(in);
  TileRecord r;

  Assert::That(reader.Read(&r), Equals(false));
  Assert::That(reader.Truncated(), Equals(true));
  Assert::That(reader.TruncatedWhat(), Contains("header"));
}

It(should_report_truncated_collider_fields) {
  std::istringstream in("tiles sheet 32 32 0 0 0 0 0 1 1 1 30\n");

  TileRecordReader reader(in);
  TileRecord r;

  Assert::That(reader.Read(&r), Equals(false));
  Assert::That(reader.Truncated(), Equals(true));
  Assert::That(reader.TruncatedWhat(), Contains("collider"));
}

It(should_report_truncated_animation_fields) {
  std::istringstream in("tiles sheet 32 32 0 0 0 0 0 1 1 0 1 4 8\n");

  TileRecordReader reader(in);
  TileRecord r;

  Assert::That(reader.Read(&r), Equals(false));
  Assert::That(reader.Truncated(), Equals(true));
  Assert::That(reader.TruncatedWhat(), Contains("animation"));
}

It(should_report_end_of_file_as_not_truncated) {
  std::istringstream in("");

  TileRecordReader reader(in);
  TileRecord r;

  Assert::That(reader.Read(&r), Equals(false));
  Assert::That(reader.Truncated(), Equals(false));
}

// ── round trip ────────────────────────────────────────────────────────
//
// The writer is the editor's SaveMap and the reader is the engine's loader,
// so a record that survives Write -> Read unchanged is the whole claim: one
// implementation, so the two can no longer drift apart.

It(should_round_trip_a_full_record) {
  const TileRecord original = FullRecord();

  std::ostringstream out;
  WriteTileRecord(out, original);
  std::istringstream in(out.str());

  TileRecordReader reader(in);
  TileRecord got;
  Assert::That(reader.Read(&got), Equals(true));

  Assert::That(got.group, Equals(original.group));
  Assert::That(got.assetId, Equals(original.assetId));
  Assert::That(got.tileWidth, Equals(original.tileWidth));
  Assert::That(got.tileHeight, Equals(original.tileHeight));
  Assert::That(got.srcRectX, Equals(original.srcRectX));
  Assert::That(got.srcRectY, Equals(original.srcRectY));
  Assert::That(got.zIndex, Equals(original.zIndex));
  Assert::That(got.worldX, Equals(original.worldX));
  Assert::That(got.worldY, Equals(original.worldY));
  Assert::That(got.scaleX, Equals(original.scaleX));
  Assert::That(got.scaleY, Equals(original.scaleY));
  Assert::That(got.collider, Equals(original.collider));
  Assert::That(got.colliderWidth, Equals(original.colliderWidth));
  Assert::That(got.colliderHeight, Equals(original.colliderHeight));
  Assert::That(got.colliderOffsetX, Equals(original.colliderOffsetX));
  Assert::That(got.colliderOffsetY, Equals(original.colliderOffsetY));
  Assert::That(got.animated, Equals(original.animated));
  Assert::That(got.numFrames, Equals(original.numFrames));
  Assert::That(got.frameSpeed, Equals(original.frameSpeed));
  Assert::That(got.vertical, Equals(original.vertical));
  Assert::That(got.looped, Equals(original.looped));
  Assert::That(got.frameOffset, Equals(original.frameOffset));
}

It(should_round_trip_a_bare_record) {
  TileRecord original;
  original.group = "tiles";
  original.assetId = "sheet";

  std::ostringstream out;
  WriteTileRecord(out, original);
  std::istringstream in(out.str());

  TileRecordReader reader(in);
  TileRecord got;
  Assert::That(reader.Read(&got), Equals(true));
  Assert::That(got.assetId, Equals("sheet"));
  Assert::That(got.collider, Equals(false));
  Assert::That(got.animated, Equals(false));
}

It(should_round_trip_several_records_in_order) {
  std::ostringstream out;
  TileRecord a;
  a.group = "tiles";
  a.assetId = "first";
  a.worldX = 10.0f;
  TileRecord b;
  b.group = "tiles";
  b.assetId = "second";
  b.worldX = 20.0f;
  b.collider = true;
  b.colliderWidth = 8;
  TileRecord c;
  c.group = "tiles";
  c.assetId = "third";
  c.worldX = 30.0f;
  c.animated = true;
  c.numFrames = 2;
  WriteTileRecord(out, a);
  WriteTileRecord(out, b);
  WriteTileRecord(out, c);

  std::istringstream in(out.str());
  TileRecordReader reader(in);
  TileRecord got;

  Assert::That(reader.Read(&got), Equals(true));
  Assert::That(got.assetId, Equals("first"));
  Assert::That(reader.Read(&got), Equals(true));
  Assert::That(got.assetId, Equals("second"));
  Assert::That(got.colliderWidth, Equals(8));
  Assert::That(reader.Read(&got), Equals(true));
  Assert::That(got.assetId, Equals("third"));
  Assert::That(got.numFrames, Equals(2));
  Assert::That(reader.Read(&got), Equals(false));
  Assert::That(reader.Truncated(), Equals(false));
}

It(should_write_the_version_header_through_the_same_function) {
  std::ostringstream out;
  WriteTileMapVersionLine(out);

  std::istringstream in(out.str());
  const TileMapVersion version = ReadTileMapVersion(in);

  Assert::That(version.declared, Equals(kTileMapFormatVersion));
  Assert::That(TileMapVersionRefusal(version, "x").empty(), Equals(true));
}

It(should_leave_a_versioned_map_readable_after_its_header) {
  // The editor's LoadMap used to start reading records immediately, so it
  // read "storm-map" as the first record's group name and loaded zero tiles
  // out of every map the engine itself wrote.
  std::ostringstream out;
  WriteTileMapVersionLine(out);
  TileRecord r;
  r.group = "tiles";
  r.assetId = "sheet";
  WriteTileRecord(out, r);

  std::istringstream in(out.str());
  const TileMapVersion version = ReadTileMapVersion(in);
  Assert::That(TileMapVersionRefusal(version, "x").empty(), Equals(true));

  TileRecordReader reader(in);
  TileRecord got;
  Assert::That(reader.Read(&got), Equals(true));
  Assert::That(got.assetId, Equals("sheet"));
}

It(should_write_a_line_per_record) {
  std::ostringstream out;
  TileRecord r;
  r.group = "tiles";
  r.assetId = "sheet";
  WriteTileRecord(out, r);
  WriteTileRecord(out, r);

  // Two records means two newlines: the flag-omitted last record is only
  // distinguishable from truncation because the line ends there.
  std::size_t newlines = 0;
  for (const char c : out.str()) {
    if (c == '\n') {
      ++newlines;
    }
  }
  Assert::That(newlines, Equals(2u));
}

It(should_write_collider_fields_only_when_the_flag_is_set) {
  TileRecord r;
  r.collider = false;
  r.colliderWidth = 99;

  std::ostringstream out;
  WriteTileRecord(out, r);

  // A stray 99 must not appear: the reader would swallow it as the animation
  // flag.
  Assert::That(out.str().find("99"), Equals(std::string::npos));
}

It(should_write_animation_fields_only_when_the_flag_is_set) {
  TileRecord r;
  r.animated = false;
  r.numFrames = 99;

  std::ostringstream out;
  WriteTileRecord(out, r);

  Assert::That(out.str().find("99"), Equals(std::string::npos));
}
}
;