#pragma once

#include <functional>
#include <string>
#include <vector>

#include "../logger.h"
#include "netConnection.h"
#include "netSocket.h"

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
    int GetClientCount() const;
    bool IsClientConnected(int clientId) const;
    NetAddress GetClientAddress(int clientId) const;
    uint16_t GetPort() const { return sock_.GetBoundPort(); }

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
