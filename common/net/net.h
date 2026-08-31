#pragma once

// ── Storm Engine networking ─────────────────────────────────────────────────
// Everything a game needs to host, join, and replicate over UDP. See
// docs/networking.md for the wire format and integration recipes.
//
//   NetServer  — host a game or run a dedicated server (slots, handshake, bans)
//   NetClient  — join a server, keep snapshots for prediction
//   NetConnection — reliable transport (vital chunks, acks, resends)
//   NetSnapshot / NetSnapshotDelta — tick state replication
//   NetMessageWriter / NetMessageReader — game-defined message packing

#include "netClient.h"
#include "netConnection.h"
#include "netPacket.h"
#include "netServer.h"
#include "netSnapshot.h"
#include "netSocket.h"
#include "netTypes.h"
#include "netVarInt.h"

namespace storm {

} // namespace storm
