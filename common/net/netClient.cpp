#include <cstring>

#include "netClient.h"

NetClient::NetClient() {
  conn_.SetSendFunc([this](const uint8_t *data, int size) {
    return sock_.Send(serverAddr_, data, size);
  });
}

NetClient::~NetClient() { Disconnect(""); }

bool NetClient::Connect(const std::string &host, uint16_t port) {
  Disconnect("");
  if (!NetResolveAddress(host, port, serverAddr_)) {
    logger_.Err("NetClient: cannot resolve host '" + host + "'");
    return false;
  }
  if (!sock_.Open(0)) {
    logger_.Err("NetClient: failed to open socket");
    return false;
  }
  TokenToNonce(NetRandom32(), clientNonce_);
  step_ = 1;
  online_ = false;
  notified_ = false;
  connectStartMs_ = NetNowMs();
  lastHandshakeMs_ = connectStartMs_;
  SendControl(kNetControlConnect, clientNonce_, 4);
  logger_.Log("NetClient: connecting to " + NetAddressToString(serverAddr_));
  return true;
}

void NetClient::Disconnect(const std::string &reason) {
  if (online_ || step_ != 0) {
    uint8_t buf[kNetMaxChunkSize];
    int len = (int)reason.size();
    if (len >= (int)sizeof(buf))
      len = (int)sizeof(buf) - 1;
    std::memcpy(buf, reason.c_str(), len);
    buf[len] = '\0';
    SendControl(kNetControlClose, buf, len + 1);
  }
  Fail(reason);
}

void NetClient::Fail(const std::string &reason) {
  bool wasActive = online_ || step_ != 0;
  conn_.Stop();
  online_ = false;
  step_ = 0;
  sock_.Close();
  cache_.Reset();
  if (wasActive && !notified_) {
    notified_ = true;
    if (onDisconnect_)
      onDisconnect_(reason.empty() ? "closed" : reason);
  }
}

void NetClient::Update() {
  uint32_t now = NetNowMs();
  if (online_) {
    conn_.Update(now);
    if (conn_.GetState() == NetConnection::kError) {
      logger_.Err("NetClient: connection lost (" +
                  std::string(conn_.GetError()) + ")");
      Fail(conn_.GetError());
    }
    return;
  }
  if (step_ == 0)
    return;
  if (now - lastHandshakeMs_ >= kNetHandshakeRetryMs) {
    lastHandshakeMs_ = now;
    if (step_ == 1) {
      SendControl(kNetControlConnect, clientNonce_, 4);
    } else if (step_ == 2) {
      uint8_t payload[8];
      std::memcpy(payload, clientNonce_, 4);
      std::memcpy(payload + 4, serverNonce_, 4);
      SendControl(kNetControlConnectReady, payload, 8);
    }
  }
  if (now - connectStartMs_ > kNetTimeoutMs)
    Fail("connection timed out");
}

void NetClient::Poll() {
  uint8_t buf[kNetMaxPacketSize];
  for (;;) {
    NetAddress from;
    int n = sock_.Recv(from, buf, sizeof(buf));
    if (n < 0)
      break;
    if (from != serverAddr_)
      continue; // ignore stray traffic
    if (NetControlPacket::IsControl(buf, n)) {
      NetControlPacket ctrl;
      if (ctrl.Unpack(buf, n))
        ProcessControl(ctrl);
    } else if (online_) {
      ProcessData(buf, n);
    }
  }
  if (online_)
    conn_.Flush(false);
}

bool NetClient::Send(const void *data, int size, bool vital) {
  if (!online_)
    return false;
  bool ok = conn_.QueueChunk(vital, data, size);
  if (vital)
    conn_.Flush(false);
  return ok;
}

void NetClient::SendControl(int message, const uint8_t *payload,
                            int payloadSize) {
  if (payloadSize < 0)
    return;
  if (payloadSize > kNetMaxPayload)
    payloadSize = kNetMaxPayload; // truncate: never overflow the frame
  NetControlPacket ctrl;
  ctrl.message = message;
  if (payload && payloadSize > 0)
    std::memcpy(ctrl.payload, payload, payloadSize);
  ctrl.payloadSize = payloadSize;
  uint8_t buf[kNetMaxPacketSize];
  int size = 0;
  if (ctrl.Pack(buf, sizeof(buf), size))
    sock_.Send(serverAddr_, buf, size);
}

void NetClient::ProcessControl(const NetControlPacket &ctrl) {
  switch (ctrl.message) {
  case kNetControlConnectAccept:
    if (online_ || step_ == 0 || ctrl.payloadSize < 4)
      return;
    // Server heard us (or restarted): adopt its cookie and prove we can
    // receive at this address.
    std::memcpy(serverNonce_, ctrl.payload, 4);
    step_ = 2;
    lastHandshakeMs_ = NetNowMs();
    {
      uint8_t payload[8];
      std::memcpy(payload, clientNonce_, 4);
      std::memcpy(payload + 4, serverNonce_, 4);
      SendControl(kNetControlConnectReady, payload, 8);
    }
    break;

  case kNetControlAccept:
    if (online_ || step_ != 2 || ctrl.payloadSize < 4)
      return;
    if (std::memcmp(ctrl.payload, serverNonce_, 4) != 0) {
      // A different server incarnation: restart the handshake.
      step_ = 1;
      SendControl(kNetControlConnect, clientNonce_, 4);
      return;
    }
    online_ = true;
    notified_ = false;
    conn_.Start(NetNowMs(), NonceToToken(clientNonce_),
                NonceToToken(serverNonce_));
    logger_.Log("NetClient: connected to " + NetAddressToString(serverAddr_));
    if (onConnect_)
      onConnect_();
    break;

  case kNetControlClose: {
    std::string reason = "closed by server";
    if (ctrl.payloadSize > 1)
      reason.assign((const char *)ctrl.payload, ctrl.payloadSize - 1);
    logger_.Log("NetClient: closed (" + reason + ")");
    Fail(reason);
    break;
  }

  default:
    break;
  }
}

void NetClient::ProcessData(const uint8_t *data, int size) {
  if (conn_.Feed(data, size) > 0) {
    NetChunk chunk;
    while (conn_.NextChunk(chunk)) {
      if (onChunk_)
        onChunk_(chunk);
    }
  }
}
