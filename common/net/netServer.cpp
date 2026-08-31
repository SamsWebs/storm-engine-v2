#include <cstring>
#include <unordered_map>

#include "netPacket.h"
#include "netServer.h"

namespace {

// Per-NetServer settings that cannot live on NetServer itself: games allocate
// the server with std::make_unique<NetServer>(), so sizeof(NetServer) is
// emitted in game code and 1.x may not change it. Keyed on `this` and erased
// by ~NetServer — without that erase, a recycled address hands the next
// server this one's settings.
//
// Not thread-safe, in keeping with the rest of the networking layer.
std::unordered_map<const NetServer *, int> &MaxClientsPerIpTable() {
  // Intentionally leaked. No in-tree consumer destroys a NetServer during
  // static teardown today, but a game is free to hold one in a static, and a
  // function-local static here would already be gone by the time such a
  // ~NetServer ran. Leaking it makes the destructor safe at any point in the
  // program's life. One map, freed by the OS at exit.
  static auto &table = *new std::unordered_map<const NetServer *, int>();
  return table;
}

} // namespace

NetServer::NetServer() = default;

NetServer::~NetServer() {
  MaxClientsPerIpTable().erase(this);
  Stop();
}

void NetServer::SetMaxClientsPerIp(int limit) {
  if (limit < 1) {
    logger_.Err("NetServer::SetMaxClientsPerIp: limit " +
                std::to_string(limit) +
                " is below 1; ignoring. The per-address cap is unchanged.");
    return;
  }
  if (limit > kMaxClients) {
    limit = kMaxClients;
  }
  MaxClientsPerIpTable()[this] = limit;
}

int NetServer::GetMaxClientsPerIp() const {
  auto found = MaxClientsPerIpTable().find(this);
  return found == MaxClientsPerIpTable().end() ? kNetMaxClientsPerIp
                                               : found->second;
}

bool NetServer::Start(uint16_t port, int maxClients) {
  Stop();
  if (maxClients < 1 || maxClients > kMaxClients) {
    logger_.Err("NetServer: maxClients out of range (1.." +
                std::to_string(kMaxClients) + ")");
    return false;
  }
  maxClients_ = maxClients;
  if (!sock_.Open(port)) {
    logger_.Err("NetServer: failed to open socket");
    return false;
  }
  bans_.clear();
  for (int i = 0; i < kMaxClients; i++) {
    slots_[i].used = false;
    slots_[i].online = false;
    slots_[i].step = 0;
    slots_[i].conn.SetSendFunc([this, i](const uint8_t *data, int size) {
      return sock_.Send(slots_[i].addr, data, size);
    });
  }
  logger_.Log("NetServer: listening on port " + std::to_string(GetPort()) +
              " (" + std::to_string(maxClients_) + " slots)");
  return true;
}

void NetServer::Stop() {
  for (int i = 0; i < kMaxClients; i++) {
    if (slots_[i].used) {
      if (slots_[i].online && onDisconnect_)
        onDisconnect_(i, "server stopped");
      slots_[i].used = false;
      slots_[i].online = false;
      slots_[i].conn.Stop();
    }
  }
  bans_.clear();
  sock_.Close();
}

void NetServer::Update() {
  uint32_t now = NetNowMs();
  for (auto it = bans_.begin(); it != bans_.end();) {
    if (now >= it->untilMs)
      it = bans_.erase(it);
    else
      ++it;
  }
  for (int i = 0; i < kMaxClients; i++) {
    Slot &slot = slots_[i];
    if (!slot.used)
      continue;
    if (slot.online) {
      slot.conn.Update(now);
      if (slot.conn.GetState() == NetConnection::kError) {
        logger_.Log("NetServer: client " + std::to_string(i) + " lost (" +
                    slot.conn.GetError() + ")");
        FreeSlot(&slot, slot.conn.GetError());
      }
    } else {
      // Handshake in progress.
      if (now - slot.lastRecvMs > kNetTimeoutMs) {
        if (slot.step == 1) // connect flood: never completed the cookie
          BanIp(NetIpToHost(slot.addr), 60);
        logger_.Log("NetServer: handshake from " +
                    NetAddressToString(slot.addr) + " timed out");
        FreeSlot(&slot, "handshake timeout");
      } else if (now - slot.lastSendMs >= kNetHandshakeRetryMs) {
        // Only step 1 is ever reachable here: step 2 is set together with
        // online (see ProcessControl), so a step-2 slot takes the branch
        // above. Its ACCEPT is retransmitted on demand instead, when the
        // client re-sends CONNECT_READY.
        slot.lastSendMs = now;
        if (slot.step == 1)
          SendControl(slot.addr, kNetControlConnectAccept, slot.serverNonce, 4);
      }
    }
  }
}

void NetServer::Poll() {
  uint8_t buf[kNetMaxPacketSize];
  for (;;) {
    NetAddress from;
    int n = sock_.Recv(from, buf, sizeof(buf));
    if (n < 0)
      break;
    if (NetControlPacket::IsControl(buf, n)) {
      NetControlPacket ctrl;
      if (ctrl.Unpack(buf, n))
        ProcessControl(from, ctrl);
    } else {
      Slot *slot = FindSlot(from);
      if (slot && slot->online)
        ProcessData(slot, buf, n);
    }
  }
  // Frame boundary: flush queued unreliable chunks.
  for (int i = 0; i < kMaxClients; i++)
    if (slots_[i].online)
      slots_[i].conn.Flush(false);
}

bool NetServer::Send(int clientId, const void *data, int size, bool vital) {
  if (clientId < 0 || clientId >= kMaxClients || !slots_[clientId].online)
    return false;
  bool ok = slots_[clientId].conn.QueueChunk(vital, data, size);
  if (vital)
    slots_[clientId].conn.Flush(false);
  return ok;
}

bool NetServer::Broadcast(const void *data, int size, bool vital) {
  bool any = false;
  for (int i = 0; i < kMaxClients; i++) {
    if (slots_[i].online && slots_[i].conn.QueueChunk(vital, data, size)) {
      any = true;
      if (vital)
        slots_[i].conn.Flush(false);
    }
  }
  return any;
}

void NetServer::DisconnectClient(int clientId, const std::string &reason) {
  if (clientId < 0 || clientId >= kMaxClients || !slots_[clientId].used)
    return;
  SendControl(slots_[clientId].addr, kNetControlClose, reason.c_str(),
              (int)reason.size() + 1);
  FreeSlot(&slots_[clientId], reason);
}

void NetServer::BanIp(uint32_t ipHost, uint32_t seconds) {
  uint32_t until = NetNowMs() + seconds * 1000;
  for (Ban &b : bans_) {
    if (b.ipHost == ipHost) {
      b.untilMs = until;
      return;
    }
  }
  bans_.push_back({ipHost, until});
  logger_.Log("NetServer: banned ip " + std::to_string(ipHost) + " for " +
              std::to_string(seconds) + "s");
}

int NetServer::GetClientCount() const {
  int count = 0;
  for (int i = 0; i < kMaxClients; i++)
    if (slots_[i].online)
      count++;
  return count;
}

int NetServer::GetConnectedClientIds(int *out, int maxOut) const {
  if (out == nullptr || maxOut < 1) {
    return 0;
  }
  int count = 0;
  for (int i = 0; i < kMaxClients && count < maxOut; i++) {
    if (IsClientConnected(i)) {
      out[count++] = i;
    }
  }
  return count;
}

bool NetServer::IsClientConnected(int clientId) const {
  return clientId >= 0 && clientId < kMaxClients && slots_[clientId].online;
}

NetAddress NetServer::GetClientAddress(int clientId) const {
  if (clientId >= 0 && clientId < kMaxClients)
    return slots_[clientId].addr;
  return NetAddress();
}

NetServer::Slot *NetServer::FindSlot(const NetAddress &addr) {
  for (int i = 0; i < kMaxClients; i++)
    if (slots_[i].used && slots_[i].addr == addr)
      return &slots_[i];
  return nullptr;
}

NetServer::Slot *NetServer::FindFreeSlot() {
  for (int i = 0; i < kMaxClients; i++)
    if (!slots_[i].used)
      return &slots_[i];
  return nullptr;
}

int NetServer::CountSlotsWithIp(const NetAddress &addr) const {
  int count = 0;
  for (int i = 0; i < kMaxClients; i++)
    if (slots_[i].used && slots_[i].addr.ip == addr.ip)
      count++;
  return count;
}

bool NetServer::IsBanned(const NetAddress &addr, uint32_t nowMs) const {
  for (const Ban &b : bans_)
    if (b.ipHost == NetIpToHost(addr) && nowMs < b.untilMs)
      return true;
  return false;
}

void NetServer::FreeSlot(Slot *slot, const std::string &reason) {
  int id = (int)(slot - slots_);
  if (slot->online && onDisconnect_)
    onDisconnect_(id, reason);
  slot->conn.Stop();
  slot->used = false;
  slot->online = false;
  slot->step = 0;
}

void NetServer::SendControl(const NetAddress &addr, int message,
                            const void *payload, int payloadSize) {
  NetSendControl(sock_, addr, message, payload, payloadSize);
}

void NetServer::HandleConnect(const NetAddress &from,
                              const uint8_t *clientNonce) {
  uint32_t now = NetNowMs();
  if (IsBanned(from, now)) {
    SendControl(from, kNetControlClose, "banned", 7);
    return;
  }
  if (Slot *existing = FindSlot(from)) {
    // An online slot is left strictly alone. CONNECT arrives before any nonce
    // is agreed, so it is the one datagram that cannot be authenticated:
    // re-arming a live session on one lets a delayed or spoofed copy from the
    // player's own address reset the handshake and run the accept path twice,
    // firing a second onConnect_ for a clientId the game still holds seated.
    // A client that really did restart sends CLOSE on the way out, and one
    // that died without sending it is reaped by the connection timeout.
    if (existing->online)
      return;
    // A genuine mid-handshake retry (step 1) answers with the nonce this slot
    // was already issued: the client re-sends CONNECT every retry until
    // CONNECT_ACCEPT lands, and minting a fresh one each time handed out an
    // unlimited supply of generator samples (P9).
    //
    // Any other step must rotate instead. The server nonce IS the connection
    // token, it travels in cleartext in every connected packet header, and
    // equality against it is the only authentication on an inbound connected
    // packet. Reusing it would let anyone who captured one datagram replay
    // the token through a spoofed CONNECT/CONNECT_READY and seize the slot.
    // No such step is reachable today (a used slot is at 1, or at 2 and
    // therefore online); this stands guard if that ever changes.
    if (existing->step != 1)
      TokenToNonce(NetNonce32(), existing->serverNonce);
    std::memcpy(existing->clientNonce, clientNonce, 4);
    existing->step = 1;
    existing->lastRecvMs = now;
    existing->lastSendMs = now;
    SendControl(from, kNetControlConnectAccept, existing->serverNonce, 4);
    return;
  }
  int usedSlots = 0;
  for (int i = 0; i < kMaxClients; i++)
    if (slots_[i].used)
      usedSlots++;
  if (usedSlots >= maxClients_) {
    SendControl(from, kNetControlClose, "server full", 12);
    return;
  }
  if (CountSlotsWithIp(from) >= GetMaxClientsPerIp()) {
    SendControl(from, kNetControlClose, "too many connections", 21);
    return;
  }
  Slot *slot = FindFreeSlot();
  if (!slot) {
    SendControl(from, kNetControlClose, "server full", 12);
    return;
  }
  slot->used = true;
  slot->online = false;
  slot->addr = from;
  slot->step = 1;
  slot->lastRecvMs = now;
  slot->lastSendMs = now;
  std::memcpy(slot->clientNonce, clientNonce, 4);
  TokenToNonce(NetNonce32(), slot->serverNonce);
  SendControl(from, kNetControlConnectAccept, slot->serverNonce, 4);
}

void NetServer::ProcessControl(const NetAddress &from,
                               const NetControlPacket &ctrl) {
  switch (ctrl.message) {
  case kNetControlConnect:
    if (ctrl.payloadSize >= 4)
      HandleConnect(from, ctrl.payload);
    break;

  case kNetControlConnectReady: {
    Slot *slot = FindSlot(from);
    if (!slot) {
      // The server restarted; fall back to a fresh handshake.
      if (ctrl.payloadSize >= 4)
        HandleConnect(from, ctrl.payload);
      return;
    }
    if (ctrl.payloadSize < 8 || (slot->step != 1 && slot->step != 2))
      return;
    if (std::memcmp(ctrl.payload, slot->clientNonce, 4) != 0 ||
        std::memcmp(ctrl.payload + 4, slot->serverNonce, 4) != 0)
      return; // stale or forged cookie
    if (slot->step == 2) {
      // ACCEPT is the last datagram of the handshake and nothing acks it, so
      // a client that never saw it keeps re-sending CONNECT_READY. Answer
      // every one — but the slot is already online, so this must not re-run
      // the accept path or onConnect_ would fire twice for one join.
      SendControl(from, kNetControlAccept, slot->serverNonce, 4);
      return;
    }
    slot->step = 2;
    slot->lastRecvMs = NetNowMs();
    slot->conn.Start(NetNowMs(), NonceToToken(slot->serverNonce),
                     NonceToToken(slot->clientNonce));
    slot->online = true;
    SendControl(from, kNetControlAccept, slot->serverNonce, 4);
    int id = (int)(slot - slots_);
    logger_.Log("NetServer: client " + std::to_string(id) + " connected (" +
                NetAddressToString(from) + ")");
    if (onConnect_)
      onConnect_(id);
    break;
  }

  case kNetControlClose: {
    Slot *slot = FindSlot(from);
    if (!slot)
      break;
    // Closing a live session is authenticated exactly like opening one: the
    // client's own cookie pair, in the clear, ahead of the reason. Matching
    // on the source address alone let one spoofed datagram kick any player
    // whose IP:port an attacker could see, which handed back everything the
    // cookie handshake was there to protect.
    //
    // A slot that is not yet online is exempt: it holds no game state, so
    // freeing it cannot kick anyone, and a client that aborts before
    // CONNECT_ACCEPT has no server nonce to quote. Making it wait out the
    // handshake timeout instead would earn it a 60 s IP ban.
    if (slot->online &&
        (ctrl.payloadSize < 8 ||
         std::memcmp(ctrl.payload, slot->clientNonce, 4) != 0 ||
         std::memcmp(ctrl.payload + 4, slot->serverNonce, 4) != 0))
      break;
    char reason[128] = "closed by client";
    int len = ctrl.payloadSize - 8;
    if (len > 1 && len < (int)sizeof(reason)) {
      std::memcpy(reason, ctrl.payload + 8, len);
      reason[len] = '\0';
    }
    FreeSlot(slot, reason);
    break;
  }

  default:
    break;
  }
}

void NetServer::ProcessData(Slot *slot, const uint8_t *data, int size) {
  if (slot->conn.Feed(data, size) > 0) {
    NetChunk chunk;
    while (slot->conn.NextChunk(chunk)) {
      if (onChunk_)
        onChunk_((int)(slot - slots_), chunk);
    }
  }
}
