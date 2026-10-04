// version.h — 2.4.4.
//
// The release number was hand-written in four places and the release workflow
// validated one of them, so a tag could go out green while the project page
// advertised the previous release and the download command on that page copied
// a package filename for a version that no longer existed. This header exists
// so a BINARY can answer for itself, and the generator that writes it exists
// so the number is written once.
//
// These cases are about the relationship between the constants and the source
// of truth, not about the literal "2.3.1" -- a spec that pins the number is a
// spec that fails every release and gets deleted. What must hold is that the
// triple agrees with the string, that the "v" form is exactly the string with
// a prefix, and that the comparison is exact.
#include <igloo/igloo_alt.h>

#include <cstring>
#include <string>

#include "../common/version.h"

using namespace igloo;
using namespace storm;

// The same declaration the generator emits, from Makefile.debian's VERSION.
// Read the source rather than restating the number, so a version bump does not
// make this file stale.
static std::string DeclaredVersion() {
  const std::string mk = [] {
    FILE *f = std::fopen("Makefile.debian", "r");
    std::string out;
    if (f == nullptr) {
      return out;
    }
    char buf[512];
    while (std::fgets(buf, sizeof(buf), f) != nullptr) {
      out += buf;
    }
    std::fclose(f);
    return out;
  }();
  const std::string key = "VERSION ?= ";
  const std::size_t at = mk.find(key);
  if (at == std::string::npos) {
    return "";
  }
  const std::size_t start = at + key.size();
  const std::size_t end = mk.find_first_of(" \t\r\n#", start);
  return mk.substr(start, end - start);
}

Describe(VersionHeaderSpec) {

  It(agrees_with_the_single_source_in_Makefile_debian) {
    // The whole point. If this drifts, a binary reports a version the release
    // never shipped, which is worse than reporting nothing.
    const std::string declared = DeclaredVersion();
    Assert::That(declared.empty(), Equals(false));
    Assert::That(std::string(kEngineVersion), Equals(declared));
  };

  It(has_a_triple_that_matches_its_string) {
    // Three constants that disagree with the string beside them are worse than
    // one constant: a consumer that trusts the triple gets a different answer
    // from one that prints the string, with nothing to say which is right.
    const std::string v = kEngineVersion;
    const std::size_t first = v.find('.');
    const std::size_t second = v.find('.', first + 1);
    Assert::That(first != std::string::npos, Equals(true));
    Assert::That(second != std::string::npos, Equals(true));

    Assert::That(v.substr(0, first),
                 Equals(std::to_string(kEngineVersionMajor)));
    Assert::That(v.substr(first + 1, second - first - 1),
                 Equals(std::to_string(kEngineVersionMinor)));
    Assert::That(v.substr(second + 1),
                 Equals(std::to_string(kEngineVersionPatch)));
  };

  It(VersionString_is_the_version_with_a_v_and_nothing_else) {
    // A hand-written "v" prefix at a call site is the half that gets
    // forgotten; a hand-written version NUMBER is the half that rots.
    Assert::That(std::string(kEngineVersionString),
                 Equals("v" + std::string(kEngineVersion)));
    Assert::That(std::string(VersionString()),
                 Equals(std::string(kEngineVersionString)));
  };

  It(compares_versions_exactly_and_refuses_null) {
    Assert::That(VersionEquals(kEngineVersion), Equals(true));
    Assert::That(VersionEquals(nullptr), Equals(false));
    Assert::That(VersionEquals(""), Equals(false));
    // A prefix is not equal: "2.3" is not this version, and treating it as
    // equal is how a 2.3.10 consumer believes it is talking to 2.3.1.
    Assert::That(VersionEquals("2.3"), Equals(false));
    Assert::That(VersionEquals((std::string(kEngineVersion) + "0").c_str()),
                 Equals(false));
    Assert::That(VersionEquals((std::string(kEngineVersion) + "-rc1").c_str()),
                 Equals(false));
  };

  It(is_a_header_only_constant_so_a_crash_banner_can_print_it) {
    // The reason the "v" form is its own constant rather than composed at
    // runtime: a crash handler is not a place to allocate, and a version
    // string that needs an allocator is a version string that is missing when
    // the process is in the state you wanted it in.
    static_assert(kEngineVersionMajor >= 0, "major is a version component");
    Assert::That(std::strlen(kEngineVersionString) > 1, Equals(true));
  };
};
