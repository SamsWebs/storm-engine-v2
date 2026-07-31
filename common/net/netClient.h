#pragma once

#include <functional>
#include <string>

#include "../logger.h"
#include "netConnection.h"
#include "netSnapshot.h"
#include "netSocket.h"

// ── Game client / joiner ────────────────────────────────────────────────────
// Connects to a NetServer over UDP: resolves the host, runs the cookie
// handshake (CONNECT -> CONNECT_ACCEPT -> CONNECT_READY -> ACCEPT, each step
// re-sent until it advances), then exchanges reliable/unreliable chunks and
// keeps recent snapshots for prediction.
//
// Loop per frame:
//   client.Update();          // handshake retries, timeouts, keepalives
//   client.Poll();            // receive, dispatch, flush unreliable sends

class NetClient {
public:
    using ConnectCallback = std::function<void()>;
    using DisconnectCallback = std::function<void(const std::string &reason)>;
    using ChunkCallback = std::function<void(const NetChunk &chunk)>;

    NetClient();
    ~NetClient();

    bool Connect(const std::string &host, uint16_t port);
    void Disconnect(const std::string &reason = "");
    void Update();
    void Poll();

    // Reliable messages are flushed immediately; unreliable messages queue
    // and go out at the end of Poll().
    bool Send(const void *data, int size, bool vital);

    bool IsConnected() const { return online_; }
    uint32_t GetRTT() const { return conn_.GetRTT(); }
    NetAddress GetServerAddress() const { return serverAddr_; }

    // Snapshot cache: store the snapshot the game reconstructs from a delta;
    // query any recent tick to predict from.
    void StoreSnapshot(int tick, const NetSnapshot &snap) { cache_.Store(tick, snap); }
    const NetSnapshot *GetSnapshot(int tick) const { return cache_.Get(tick); }
    int GetLatestSnapshotTick() const { return cache_.GetLatestTick(); }

    void SetOnConnect(ConnectCallback cb) { onConnect_ = cb; }
    void SetOnDisconnect(DisconnectCallback cb) { onDisconnect_ = cb; }
    void SetOnChunk(ChunkCallback cb) { onChunk_ = cb; }

private:
    void SendControl(int message, const uint8_t *payload, int payloadSize);
    void ProcessControl(const NetControlPacket &ctrl);
    void ProcessData(const uint8_t *data, int size);
    void Fail(const std::string &reason);

    NetSocket sock_;
    NetConnection conn_;
    NetSnapshotCache cache_;
    NetAddress serverAddr_;
    uint8_t clientNonce_[4] = {};
    uint8_t serverNonce_[4] = {};
    int step_ = 0; // 0 idle, 1 CONNECT sent, 2 CONNECT_READY sent, 3 online
    uint32_t connectStartMs_ = 0;
    uint32_t lastHandshakeMs_ = 0;
    bool online_ = false;
    bool notified_ = false;
    ConnectCallback onConnect_;
    DisconnectCallback onDisconnect_;
    ChunkCallback onChunk_;
    Logger logger_;
};
