#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <mutex>
#include <chrono>
#include <asio.hpp>
#include "toio_control/osc/OscTransport.h"
#include "toio_control/osc/OscPacket.h"
#include "toio_control/osc/OscEncryptor.h"

namespace swarm_control {

struct ReceivedPosition {
    std::string deviceId;  // デバイスID（m5から送られてくる）
    double x, y;           // 座標 (mm)
    double angle;          // 角度 (rad)
    std::chrono::steady_clock::time_point receivedTime;
};

class OscReceiver {
public:
    // コールバック設定
    using AnnounceCallback = std::function<void(const std::string& deviceId,
                                                const std::string& mac,
                                                const std::string& firmwareVersion)>;
    using HeartbeatCallback = std::function<void(const std::string& deviceId,
                                                 int sequence,
                                                 double latencyMs)>;
    using PositionCallback = std::function<void(const ReceivedPosition&)>;
    
    OscReceiver(int listenPort);
    ~OscReceiver();
    
    void setAnnounceCallback(AnnounceCallback callback);
    void setHeartbeatCallback(HeartbeatCallback callback);
    void setPositionCallback(PositionCallback callback);
    
    void start();
    void stop();
    
    // 受信した座標を取得（ポーリング用、オプション）
    std::vector<ReceivedPosition> getReceivedPositions();
    
    // 暗号化サポート（acousticsと同様）
    void enableEncryption(const toio_control::osc::OscEncryptor::Key256& key,
                         const toio_control::osc::OscEncryptor::Iv128& iv);
    void disableEncryption();
    bool encryptionEnabled() const;

private:
    void handlePacket(const toio_control::osc::Packet& packet, 
                     const toio_control::osc::OscListener::Endpoint& endpoint);
    void processMessage(const toio_control::osc::Message& message);
    void processAnnounce(const toio_control::osc::Message& message);
    void processHeartbeat(const toio_control::osc::Message& message);
    void processPosition(const toio_control::osc::Message& message);
    
    std::unique_ptr<toio_control::osc::IoContextRunner> ioContextRunner_;
    std::unique_ptr<toio_control::osc::OscListener> listener_;
    AnnounceCallback announceCallback_;
    HeartbeatCallback heartbeatCallback_;
    PositionCallback positionCallback_;
    std::vector<ReceivedPosition> receivedPositions_;
    std::mutex positionsMutex_;
    int listenPort_;
    bool running_ = false;
    
    // 暗号化サポート（将来の拡張用、現在はOscListenerが処理）
    bool encryptionEnabled_ = false;
    toio_control::osc::OscEncryptor::Key256 encryptionKey_;
    toio_control::osc::OscEncryptor::Iv128 encryptionIv_;
};

}  // namespace swarm_control
