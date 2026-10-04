// Socket-open failure classification and libnx Result decoding — 2.7.2.
//
// The first spec file for the socket layer. Everything here is the PURE half of
// NetSocket::Open: which of three distinct stages failed, what each one says,
// and how a libnx Result is taken apart. The syscalls themselves are not
// spec'd and cannot be — the suite runs on a Linux box with a working BSD
// stack, where socket() never fails, so a spec for it would be asserting a
// number read off a diff rather than observing anything.
//
// What the classification is for: Open() used to report all three stages with
// one line, "failed to create non-blocking UDP socket". On Switch the missing
// platform init therefore surfaced as a socket-creation complaint, which reads
// like a firewall problem. A player (or a developer) chasing "the firewall is
// blocking the UDP port" is three wrong guesses away from "libnx was never
// started". The three stages are three different problems with three different
// fixes, so they get three different names.
#include <string>

#include <igloo/igloo_alt.h>

#include "../../common/net/netSocket.h"

using namespace igloo;
using namespace storm;

namespace {

// Every message Open() can log, keyed by stage. The distinctness of the three
// strings IS the assertion -- see the spec that compares them.
std::string Message(NetSocketOpenFailure failure, uint16_t port = 8303) {
  return NetSocketOpenFailureMessage(failure, port);
}

} // namespace

Describe(NetSocketOpenFailureMessages){It(says_nothing_when_nothing_failed){
    Assert::That(Message(NetSocketOpenFailure::None), Equals(std::string()));
}

It(names_the_platform_init_stage_rather_than_a_socket) {
  const std::string text = Message(NetSocketOpenFailure::PlatformInit);
  // The whole point of the split: this must not CLAIM a socket-creation
  // failure, which is what sends a reader looking at a firewall. Asserted
  // on the claim rather than on the substring "socket", because the
  // message names socketInitializeDefault and a substring test cannot tell
  // naming the platform call apart from blaming the socket. An earlier
  // version of this spec asserted find("socket") == npos and failed on
  // exactly that.
  Assert::That(text.find("failed to create") == std::string::npos, Is().True());
  // ...and it has to name the two calls that can fail, or it is only
  // one layer more specific than the line it replaced.
  Assert::That(text.find("WSAStartup") != std::string::npos, Is().True());
  Assert::That(text.find("socketInitializeDefault") != std::string::npos,
               Is().True());
}

It(names_the_socket_creation_stage) {
  const std::string text = Message(NetSocketOpenFailure::Socket);
  Assert::That(text.find("failed to create") != std::string::npos, Is().True());
  Assert::That(text.find("non-blocking") != std::string::npos, Is().True());
  Assert::That(text.find("bind") == std::string::npos, Is().True());
}

It(carries_the_port_it_failed_to_bind) {
  // The bind stage is the one a player can actually do something about,
  // so the number has to be in the line -- "something is holding a port"
  // is only actionable once you know which one.
  Assert::That(Message(NetSocketOpenFailure::Bind, 8303).find("8303") !=
                   std::string::npos,
               Is().True());
  Assert::That(Message(NetSocketOpenFailure::Bind, 27015).find("27015") !=
                   std::string::npos,
               Is().True());
}

It(distinguishes_an_ephemeral_bind_failure_from_a_named_one) {
  // Port 0 means "OS assigns one", and a log line reading "failed to bind
  // UDP port 0" is a contradiction the reader has to stop and think
  // about. It says what actually happened instead.
  const std::string text = Message(NetSocketOpenFailure::Bind, 0);
  Assert::That(text.find("port 0") == std::string::npos, Is().True());
  Assert::That(text != Message(NetSocketOpenFailure::Bind, 8303), Is().True());
}

It(keeps_the_three_failure_messages_distinct) {
  // The regression this whole file exists for. Collapse the three back
  // into one string -- which is what the code did before this slice -- and
  // this fails, because the property being bought is the DISTINCTION,
  // not any particular wording.
  const std::string platform = Message(NetSocketOpenFailure::PlatformInit);
  const std::string socket = Message(NetSocketOpenFailure::Socket);
  const std::string bind = Message(NetSocketOpenFailure::Bind);
  Assert::That(platform != socket, Is().True());
  Assert::That(socket != bind, Is().True());
  Assert::That(platform != bind, Is().True());
}

It(shares_one_prefix_so_a_grep_finds_every_net_socket_line) {
  // Log greppability is a real cost of splitting one line into three.
  const NetSocketOpenFailure all[] = {NetSocketOpenFailure::PlatformInit,
                                      NetSocketOpenFailure::Socket,
                                      NetSocketOpenFailure::Bind};
  for (NetSocketOpenFailure failure : all) {
    Assert::That(Message(failure).rfind("NetSocket: ", 0) == 0, Is().True());
  }
}

It(still_says_something_about_a_stage_this_build_does_not_know) {
  // A stage added later without a message must not become a silent
  // no-log. Unknown enumerators are reachable from a stale object file,
  // and the failure mode we are fixing here is silence.
  const auto unknown = static_cast<NetSocketOpenFailure>(99);
  const std::string text = Message(unknown);
  Assert::That(text.empty(), Is().False());
  Assert::That(text != Message(NetSocketOpenFailure::None), Is().True());
}
}
;

Describe(NetSocketSetupClassification){
    It(reports_nothing_wrong_when_the_socket_opened_and_went_nonblocking){
        Assert::That(NetClassifySocketSetup(true, true),
                     Equals(NetSocketOpenFailure::None));
}

It(blames_the_socket_when_socket_itself_failed) {
  // Whatever went wrong downstream, calling fcntl on -1 cannot have
  // helped. The message has to name the earliest thing that broke.
  Assert::That(NetClassifySocketSetup(false, true),
               Equals(NetSocketOpenFailure::Socket));
  Assert::That(NetClassifySocketSetup(false, false),
               Equals(NetSocketOpenFailure::Socket));
}

It(blames_the_socket_when_only_the_nonblocking_switch_failed) {
  // socket() succeeded and then the fd was not made non-blocking: a live
  // socket that would block the whole game inside recvfrom. Different
  // cause from the line above, same stage, because the fix is the same
  // place.
  Assert::That(NetClassifySocketSetup(true, false),
               Equals(NetSocketOpenFailure::Socket));
}
}
;

Describe(NetResultModuleAndDescription){
    It(reads_a_result_as_success_when_it_is_zero){
        // libnx spells success 0, and R_SUCCEEDED is == 0. Zero therefore has
        // to survive the decode without inventing a module.
        Assert::That(NetResultModule(0), Equals(uint32_t(0)));
Assert::That(NetResultDescription(0), Equals(uint32_t(0)));
}

It(splits_a_result_into_its_module_and_description) {
  // The bit layout is libnx's, from switch/result.h: the module is the
  // low 9 bits and the description starts at bit 9. These are not
  // round numbers a person would guess -- 0x1FF and >>9, not 0x3FF and
  // >>10 -- and getting them wrong decodes a real Switch error into
  // confident nonsense. The Switch build static_asserts the same
  // arithmetic against R_MODULE/R_DESCRIPTION; this spec is what holds
  // it on the platforms that run the suite.
  const uint32_t result = 0x1234;
  Assert::That(NetResultModule(result), Equals(uint32_t(0x1234) & 0x1FFu));
  Assert::That(NetResultDescription(result),
               Equals(uint32_t((0x1234 >> 9) & 0x1FFFu)));
}

It(keeps_a_known_libnx_module_intact) {
  // Module_Libnx is 345 in switch/result.h. A Result carrying it has to
  // decode to 345, which is a real value from the real header rather
  // than a bit-pattern exercise.
  const uint32_t result = (uint32_t(2) << 9) | 345u;
  Assert::That(NetResultModule(result), Equals(uint32_t(345)));
  Assert::That(NetResultDescription(result), Equals(uint32_t(2)));
}

It(decodes_a_kernel_result_to_its_module_number) {
  // Module_Kernel is 1.
  const uint32_t result = (uint32_t(7) << 9) | 1u;
  Assert::That(NetResultModule(result), Equals(uint32_t(1)));
  Assert::That(NetResultDescription(result), Equals(uint32_t(7)));
}
}
;

Describe(NetResultTextOutput){It(adds_nothing_at_all_for_a_successful_result){
    // This is composed onto the platform-init line, and success never
    // reaches that line -- so a "Result 0" suffix would only ever be
    // noise. Asserted because "0 means success" is the kind of thing an
    // edit to the formatter quietly breaks.
    Assert::That(NetResultText(0), Equals(std::string()));
}

It(prints_the_hex_code_so_it_can_be_looked_up) {
  // 345<<9 | 2 is a real shape of a real libnx failure, and its value is
  // 1369 -- which is 0x559. A "0x" prefix in front of a decimal 1369 is a
  // number nobody can look up, so the assertion is on the hex digits and
  // not merely on the presence of the characters "0x". Zero-padded to
  // four, because a Result is 32 bits and the width is the point.
  const std::string text = NetResultText((uint32_t(2) << 9) | 345u);
  Assert::That(text.find("0x0559") != std::string::npos, Is().True());
}

It(prints_the_module_and_description_it_decoded) {
  const std::string text = NetResultText((uint32_t(2) << 9) | 345u);
  Assert::That(text.find("libnx result") != std::string::npos, Is().True());
  Assert::That(text.find("module 345") != std::string::npos, Is().True());
  Assert::That(text.find("description 2") != std::string::npos, Is().True());
}
}
;
