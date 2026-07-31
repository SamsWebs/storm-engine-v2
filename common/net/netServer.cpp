#include <cstring>

#include "netServer.h"
#include "netPacket.h"

NetServer::NetServer() = default;

NetServer::~NetServer() {
    Stop();
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
                logger_.Log("NetServer: client " + std::to_string(i) +
                            " lost (" + slot.conn.GetError() + ")");
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
                slot.lastSendMs = now;
                if (slot.step == 1)
                    SendControl(slot.addr, kNetControlConnectAccept,
                                slot.serverNonce, 4);
                else if (slot.step == 2)
                    SendControl(slot.addr, kNetControlAccept,
                                slot.serverNonce, 4);
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
    logger_.Log("NetServer: banned ip " + std::to_string(ipHost) +
                " for " + std::to_string(seconds) + "s");
}

int NetServer::GetClientCount() const {
    int count = 0;
    for (int i = 0; i < kMaxClients; i++)
        if (slots_[i].online)
            count++;
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
    NetControlPacket ctrl;
    ctrl.message = message;
    if (payload && payloadSize > 0)
        std::memcpy(ctrl.payload, payload, payloadSize);
    ctrl.payloadSize = payloadSize;
    uint8_t buf[kNetMaxPacketSize];
    int size = 0;
    if (ctrl.Pack(buf, sizeof(buf), size))
        sock_.Send(addr, buf, size);
}

void NetServer::HandleConnect(const NetAddress &from, const uint8_t *clientNonce) {
    uint32_t now = NetNowMs();
    if (IsBanned(from, now)) {
        SendControl(from, kNetControlClose, "banned", 7);
        return;
    }
    if (Slot *existing = FindSlot(from)) {
        // The client restarted mid-handshake: refresh nonces and answer.
        std::memcpy(existing->clientNonce, clientNonce, 4);
        TokenToNonce(NetRandom32(), existing->serverNonce);
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
    if (CountSlotsWithIp(from) >= kNetMaxClientsPerIp) {
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
    TokenToNonce(NetRandom32(), slot->serverNonce);
    SendControl(from, kNetControlConnectAccept, slot->serverNonce, 4);
}

void NetServer::ProcessControl(const NetAddress &from, const NetControlPacket &ctrl) {
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
        if (ctrl.payloadSize < 8 || slot->step != 1)
            return;
        if (std::memcmp(ctrl.payload, slot->clientNonce, 4) != 0 ||
            std::memcmp(ctrl.payload + 4, slot->serverNonce, 4) != 0)
            return; // stale or forged cookie
        slot->step = 2;
        slot->lastRecvMs = NetNowMs();
        slot->conn.Start(NetNowMs(), NonceToToken(slot->serverNonce),
                         NonceToToken(slot->clientNonce));
        slot->online = true;
        SendControl(from, kNetControlAccept, slot->serverNonce, 4);
        int id = (int)(slot - slots_);
        logger_.Log("NetServer: client " + std::to_string(id) +
                    " connected (" + NetAddressToString(from) + ")");
        if (onConnect_)
            onConnect_(id);
        break;
    }

    case kNetControlClose: {
        Slot *slot = FindSlot(from);
        if (slot) {
            char reason[128] = "closed by client";
            int len = ctrl.payloadSize;
            if (len > 1 && len < (int)sizeof(reason)) {
                std::memcpy(reason, ctrl.payload, len);
                reason[len] = '\0';
            }
            FreeSlot(slot, reason);
        }
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
