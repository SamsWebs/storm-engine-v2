// The .map format version header — 2.5.1.
//
// A .map had no version, so a reader could not tell a key that was ADDED from
// one that still parses and now means something else. "Unknown keys are
// ignored" covers the first and not the second, and 2.0.0's Tile change is the
// proof this format moves: the animation fields were appended to the struct and
// to the record, and a file written before that is still a file on someone's
// disk.
//
// The version number is the small half. The half that costs anything is the
// refusal: a map declaring a version this build does not know has to be
// REFUSED, with a diagnostic, rather than half-read into a level that looks
// fine and is not. So the states below are the actual decision, and each one
// gets a case.
//
// Header-only and pure: the reader takes a std::istream, so a spec needs no
// file on disk and no filesystem, and there is no .cpp to add to
// engine-sources.txt (which matters, because a new .cpp under common/ has to be
// added to four build files or it silently does not reach Switch or Android).
#include <sstream>
#include <string>

#include <igloo/igloo_alt.h>

#include "../common/tilemapFormat.h"

using namespace igloo;
using namespace storm;

namespace {

// The whole reader is exercised through a string, because a version header is
// two tokens and there is nothing about it that needs a real file.
TileMapVersion Read(const std::string &text) {
  std::istringstream in{text};
  return ReadTileMapVersion(in);
}

const char *kTwoRecords = "tiles grass 8 8 0 0 0 0 0 1 1 0 0\n"
                          "tiles wall 8 8 16 0 1 16 24 1 1 1 8 8 2 3 0\n";

} // namespace

Describe(TileMapVersionReading) {
  It(treats_a_file_with_no_header_as_the_oldest_format) {
    // This is the whole backwards-compatibility promise: a map written
    // before the header existed keeps loading, and it keeps loading
    // WITHOUT a diagnostic, because it is not broken -- it is old.
    const TileMapVersion version = Read(kTwoRecords);
    Assert::That(version.state, Equals(TileMapVersionState::Unversioned));
    Assert::That(version.version, Equals(kTileMapFormatMinVersion));
  }

  It(does_not_claim_a_declared_version_for_a_file_that_declares_none) {
    // declared is what the file said; version is what to read it with.
    // Collapsing them would make "the file said nothing" indistinguishable
    // from "the file said 1", which is precisely the ambiguity the header
    // was added to remove.
    Assert::That(Read(kTwoRecords).declared, Equals(0));
  }

  It(leaves_the_first_record_readable_when_there_is_no_header) {
    // The failure this guards is invisible: a reader that probes the first
    // token and does not put it back parses every record one field to the
    // left, so the group name becomes the asset id and the asset id becomes
    // the tile width. Every field is still a valid int or string, so
    // nothing complains and every map loads the wrong tiles.
    std::istringstream in{std::string(kTwoRecords)};
    ReadTileMapVersion(in);
    std::string group, assetId;
    int tileW = 0;
    in >> group >> assetId >> tileW;
    Assert::That(group, Equals("tiles"));
    Assert::That(assetId, Equals("grass"));
    Assert::That(tileW, Equals(8));
  };

  It(reads_a_current_header_as_supported) {
    const TileMapVersion version =
        Read("storm-map 1\n" + std::string(kTwoRecords));
    Assert::That(version.state, Equals(TileMapVersionState::Supported));
    Assert::That(version.version, Equals(kTileMapFormatVersion));
    Assert::That(version.declared, Equals(1));
  }

  It(refuses_a_version_newer_than_this_build) {
    // The half that costs anything. Half-reading a newer format produces a
    // level that renders and is wrong, which is the worst outcome
    // available; refusing produces a build that says so.
    const TileMapVersion version = Read("storm-map 999\n");
    Assert::That(version.state, Equals(TileMapVersionState::TooNew));
    Assert::That(version.declared, Equals(999));
  }

  It(refuses_a_version_below_the_floor) {
    const TileMapVersion version = Read("storm-map 0\n");
    Assert::That(version.state, Equals(TileMapVersionState::TooOld));
  }

  It(refuses_a_negative_version_rather_than_accepting_it) {
    // "-" parses as a number, so this is the case where "did it parse" and
    // "is it usable" are different questions. Accepting -1 as a version
    // would be a file older than the format's own floor.
    const TileMapVersion version = Read("storm-map -1\n");
    Assert::That(version.state, Equals(TileMapVersionState::TooOld));
  }

  It(refuses_a_header_with_no_version_at_all) {
    Assert::That(Read("storm-map\n").state,
                 Equals(TileMapVersionState::Malformed));
  }

  It(refuses_a_header_whose_version_is_not_a_number) {
    // "storm-map next" is what a hand-edit produces. Reading it as
    // unversioned would fall through to the old parser, which is the exact
    // silent-misread this header exists to stop.
    Assert::That(Read("storm-map next\n").state,
                 Equals(TileMapVersionState::Malformed));
  }

  It(reads_a_header_from_an_empty_file_without_treating_it_as_an_error) {
    // An empty .map is a map with no tiles. The editor can produce one when
    // a canvas is empty, and refusing to load it would break Save-then-load.
    Assert::That(Read("").state, Equals(TileMapVersionState::Unversioned));
  }

  It(does_not_match_the_magic_on_a_partial_prefix) {
    // Token matching, not substring matching: "storm-maps 1" is a group
    // name that happens to start with the magic, and treating it as a
    // header would eat a record.
    const TileMapVersion version = Read("storm-maps 1\n");
    Assert::That(version.state, Equals(TileMapVersionState::Unversioned));
  }

  It(tolerates_leading_whitespace_before_the_header) {
    Assert::That(Read("   storm-map 1\n").state,
                 Equals(TileMapVersionState::Supported));
  }

  It(tolerates_a_carriage_return_from_a_file_written_on_windows) {
    // The editor is a Windows application and the format is whitespace
    // separated, so a CRLF map is a normal map, not a malformed one.
    Assert::That(Read("storm-map 1\r\n" + std::string(kTwoRecords)).state,
                 Equals(TileMapVersionState::Supported));
  }

  It(reads_the_header_as_two_tokens_so_a_lost_newline_does_not_lose_a_record) {
    // If the header were read as a LINE, a hand-edit that joined it to the
    // first record would silently drop that record. Two tokens is the
    // tolerant reading, and the first record survives either way.
    const TileMapVersion version =
        Read("storm-map 1 tiles grass 8 8 0 0 0 0 0 1 1 0 0\n");
    Assert::That(version.state, Equals(TileMapVersionState::Supported));
  }
};

Describe(TileMapVersionLineOutput){It(is_accepted_by_this_build_s_own_reader){
    // The property that makes this a fix and not two implementations. The
    // engine writes the line here and reads it here, so there is no second
    // parser to drift -- which is the point, because the editor's writer is
    // a separate binary and used to be exactly that second parser.
    std::istringstream in{TileMapVersionLine() + std::string(kTwoRecords)};
const TileMapVersion version = ReadTileMapVersion(in);
Assert::That(version.state, Equals(TileMapVersionState::Supported));
Assert::That(version.declared, Equals(kTileMapFormatVersion));
}

It(ends_with_a_newline_so_a_record_cannot_be_joined_to_it) {
  // The writer's own responsibility. Asserted because a missing newline
  // is invisible until a map has a version header, and then it eats the
  // first tile of every level.
  Assert::That(TileMapVersionLine().back(), Equals('\n'));
}

It(leaves_the_stream_positioned_on_the_first_record) {
  // If it did not, the loader would skip a tile on every versioned map
  // and every test using a versioned fixture would be one shorter.
  std::istringstream in{TileMapVersionLine() + std::string(kTwoRecords)};
  ReadTileMapVersion(in);
  std::string group;
  in >> group;
  Assert::That(group, Equals("tiles"));
}

It(declares_the_version_this_build_actually_reads) {
  // Otherwise the writer stamps one number and the reader enforces
  // another, and the round trip above still passes — which is exactly the
  // drift the shared constant prevents.
  Assert::That(TileMapVersionLine().find(
                   std::to_string(kTileMapFormatVersion)) != std::string::npos,
               Is().True());
}
}
;
