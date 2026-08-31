#pragma once

#include <functional>
#include <string>
#include <vector>

#include "../logger.h"
#include "netConnection.h"
#include "netSocket.h"

namespace storm {

// ── Game host / dedicated server ────────────────────────────────────────────
// UDP server with fixed client slots, a cookie handshake (anti-spoofing: a
// joining client must echo back a random nonce before it gets a slot, so a
// spoofed source address can never be admitted), per-IP caps, and bans.
//
// Loop per frame:
//   server.Update();          // retries, timeouts, keepalives
//   server.Poll();            // receive, dispatch, flush unreliable sends
// The game decides what a chunk means (NetMessageReader over game-defined ids).

class NetServer {
public:
    static constexpr int kMaxClients = 16;

    using ConnectCallback = std::function<void(int clientId)>;
    using DisconnectCallback = std::function<void(int clientId, const std::string &reason)>;
    using ChunkCallback = std::function<void(int clientId, const NetChunk &chunk)>;

    NetServer();
    ~NetServer();

    // Owns a socket descriptor and installs callbacks capturing `this`: a copy
    // would give two objects whose callbacks point at the original, and two
    // destructors closing one descriptor. KNOWN_ISSUES item 6, fixed in 2.0.0.
    NetServer(const NetServer &) = delete;
    NetServer &operator=(const NetServer &) = delete;

    bool Start(uint16_t port, int maxClients = 8); // port 0 = ephemeral
    void Stop();
    void Update();
    void Poll();

    // Reliable messages are flushed immediately; unreliable messages queue
    // and go out at the end of Poll(). Chunk data is copied at call time.
    bool Send(int clientId, const void *data, int size, bool vital);
    bool Broadcast(const void *data, int size, bool vital);
    void DisconnectClient(int clientId, const std::string &reason = "");
    void BanIp(uint32_t ipHost, uint32_t seconds);

    bool IsRunning() const { return sock_.IsOpen(); }

    // How many clients are connected. This is a count for display — "3/12
    // players" — and NOT a loop bound.
    //
    // Client ids are slot indices into a fixed array, not a dense range.
    // `for (int i = 0; i < GetClientCount(); i++)` works in a two-player test
    // and then silently stops sending to a player the moment anyone quits: a
    // client in slot 3 stays connected while the count reads 3. Nothing
    // errors; that player just stops receiving the world.
    //
    // Iterate with GetConnectedClientIds, or over kMaxClients guarded by
    // IsClientConnected.
    int GetClientCount() const;

    // Writes the connected client ids into `out` in ascending order and
    // returns how many were written, never more than `maxOut`. Pass
    // kMaxClients as `maxOut` to be sure of getting all of them.
    //
    //     int ids[NetServer::kMaxClients];
    //     const int count = server.GetConnectedClientIds(ids, NetServer::kMaxClients);
    //     for (int i = 0; i < count; ++i) { server.Send(ids[i], ...); }
    //
    // Takes an array rather than returning a container because this sits on
    // the per-tick send path and must not allocate.
    //
    // Returns 0 and writes nothing when `out` is null or `maxOut` is below 1.
    int GetConnectedClientIds(int *out, int maxOut) const;

    bool IsClientConnected(int clientId) const;
    NetAddress GetClientAddress(int clientId) const;
    uint16_t GetPort() const { return sock_.GetBoundPort(); }

    // How many clients may connect from one address. Defaults to
    // kNetMaxClientsPerIp (4), which is an anti-flood cap sized for a LAN.
    //
    // Over the internet every player behind one router shares a public
    // address, so a twelve-player game with two people in one house is
    // refused at the default. Raise it for internet play; leave it alone for
    // a LAN, where it is doing real work.
    //
    // A limit below 1 is refused and logged; a limit above kMaxClients is
    // clamped to kMaxClients. Takes effect on the next connection attempt,
    // and never disconnects a client already admitted.
    //
    // The setting is held outside the object: sizeof(NetServer) is ABI,
    // because games allocate the server themselves and the size is emitted
    // at their call site.
    void SetMaxClientsPerIp(int limit);
    int GetMaxClientsPerIp() const;

    void SetOnClientConnect(ConnectCallback cb) { onConnect_ = cb; }
    void SetOnClientDisconnect(DisconnectCallback cb) { onDisconnect_ = cb; }
    void SetOnChunk(ChunkCallback cb) { onChunk_ = cb; }

private:
    struct Slot {
        bool used = false;
        bool online = false;
        NetAddress addr;
        uint8_t clientNonce[4] = {};
        uint8_t serverNonce[4] = {};
        int step = 0; // 0 idle, 1 CONNECT seen, 2 CONNECT_READY verified
        uint32_t lastRecvMs = 0;
        uint32_t lastSendMs = 0;
        NetConnection conn;
    };
    struct Ban {
        uint32_t ipHost;
        uint32_t untilMs;
    };

    Slot *FindSlot(const NetAddress &addr);
    Slot *FindFreeSlot();
    int CountSlotsWithIp(const NetAddress &addr) const;
    bool IsBanned(const NetAddress &addr, uint32_t nowMs) const;
    void FreeSlot(Slot *slot, const std::string &reason);
    void SendControl(const NetAddress &addr, int message,
                     const void *payload = nullptr, int payloadSize = 0);
    void HandleConnect(const NetAddress &from, const uint8_t *clientNonce);
    void ProcessControl(const NetAddress &from, const NetControlPacket &ctrl);
    void ProcessData(Slot *slot, const uint8_t *data, int size);

    NetSocket sock_;
    Slot slots_[kMaxClients];
    int maxClients_ = 8;
    std::vector<Ban> bans_;
    ConnectCallback onConnect_;
    DisconnectCallback onDisconnect_;
    ChunkCallback onChunk_;
    Logger logger_;
};

} // namespace storm
