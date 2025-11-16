#include "osc_receiver.hpp"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cmath>

namespace swarm_control {

OscReceiver::OscReceiver(int listenPort)
    : listenPort_(listenPort), running_(false), encryptionEnabled_(false) {
    
    try {
        ioContextRunner_ = std::make_unique<toio_control::osc::IoContextRunner>();
        ioContextRunner_->start();
        
        asio::ip::address listenAddress = asio::ip::address_v4::any();
        toio_control::osc::OscListener::Endpoint endpoint(listenAddress, static_cast<unsigned short>(listenPort_));
        
        listener_ = std::make_unique<toio_control::osc::OscListener>(
            ioContextRunner_->context(),
            endpoint,
            [this](const toio_control::osc::Packet& packet, 
                   const toio_control::osc::OscListener::Endpoint& endpoint) {
                handlePacket(packet, endpoint);
            }
        );
        
        spdlog::info("OSC receiver initialized on port {}", listenPort_);
    } catch (const std::exception& ex) {
        spdlog::error("Failed to initialize OSC receiver: {}", ex.what());
    }
}

OscReceiver::~OscReceiver() {
    stop();
}

void OscReceiver::setAnnounceCallback(AnnounceCallback callback) {
    announceCallback_ = std::move(callback);
}

void OscReceiver::setHeartbeatCallback(HeartbeatCallback callback) {
    heartbeatCallback_ = std::move(callback);
}

void OscReceiver::setPositionCallback(PositionCallback callback) {
    positionCallback_ = std::move(callback);
}

void OscReceiver::enableEncryption(const toio_control::osc::OscEncryptor::Key256& key,
                                  const toio_control::osc::OscEncryptor::Iv128& iv) {
    if (listener_) {
        listener_->enableEncryption(key, iv);
        encryptionEnabled_ = true;
        encryptionKey_ = key;
        encryptionIv_ = iv;
        spdlog::info("OSC receiver encryption enabled");
    }
}

void OscReceiver::disableEncryption() {
    if (listener_) {
        listener_->disableEncryption();
        encryptionEnabled_ = false;
        spdlog::info("OSC receiver encryption disabled");
    }
}

bool OscReceiver::encryptionEnabled() const {
    if (listener_) {
        return listener_->encryptionEnabled();
    }
    return encryptionEnabled_;
}

void OscReceiver::start() {
    if (running_) {
        return;
    }
    
    try {
        if (listener_) {
            listener_->start();
            running_ = true;
            spdlog::info("OSC receiver started on port {}", listenPort_);
        }
    } catch (const std::exception& ex) {
        spdlog::error("Failed to start OSC receiver: {}", ex.what());
    }
}

void OscReceiver::stop() {
    if (!running_) {
        return;
    }
    
    try {
        if (listener_) {
            listener_->stop();
        }
        running_ = false;
        spdlog::info("OSC receiver stopped");
    } catch (const std::exception& ex) {
        spdlog::error("Failed to stop OSC receiver: {}", ex.what());
    }
}

void OscReceiver::handlePacket(const toio_control::osc::Packet& packet, 
                               const toio_control::osc::OscListener::Endpoint& endpoint) {
    try {
        if (packet.isBundle()) {
            const auto& bundle = packet.asBundle();
            for (const auto& element : bundle.elements) {
                if (element.isMessage()) {
                    const auto& message = element.asMessage();
                    processMessage(message);
                }
            }
        } else if (packet.isMessage()) {
            const auto& message = packet.asMessage();
            processMessage(message);
        }
    } catch (const std::exception& ex) {
        spdlog::warn("Error processing OSC packet: {}", ex.what());
    }
}

void OscReceiver::processMessage(const toio_control::osc::Message& message) {
    std::string address = message.address;
    
    // /announce メッセージ
    if (address == "/announce") {
        processAnnounce(message);
        return;
    }
    
    // /heartbeat メッセージ
    if (address == "/heartbeat") {
        processHeartbeat(message);
        return;
    }
    
    // /toio/position メッセージ
    if (address == "/toio/position" || address.find("/toio/") == 0) {
        processPosition(message);
        return;
    }
}

void OscReceiver::processAnnounce(const toio_control::osc::Message& message) {
    // /announce: [deviceId (string), mac (string), firmwareVersion (string)]
    if (message.arguments.size() < 3) {
        spdlog::warn("Invalid /announce message: insufficient arguments");
        return;
    }
    
    try {
        std::string deviceId;
        std::string mac;
        std::string firmwareVersion;
        
        if (std::holds_alternative<std::string>(message.arguments[0])) {
            deviceId = std::get<std::string>(message.arguments[0]);
        } else {
            spdlog::warn("Invalid /announce message: deviceId must be string");
            return;
        }
        
        if (std::holds_alternative<std::string>(message.arguments[1])) {
            mac = std::get<std::string>(message.arguments[1]);
        } else {
            spdlog::warn("Invalid /announce message: mac must be string");
            return;
        }
        
        if (std::holds_alternative<std::string>(message.arguments[2])) {
            firmwareVersion = std::get<std::string>(message.arguments[2]);
        } else {
            spdlog::warn("Invalid /announce message: firmwareVersion must be string");
            return;
        }
        
        if (announceCallback_) {
            announceCallback_(deviceId, mac, firmwareVersion);
        }
        
        spdlog::debug("Processed /announce: deviceId={}, mac={}, fw={}", 
                     deviceId, mac, firmwareVersion);
    } catch (const std::exception& ex) {
        spdlog::warn("Error processing /announce: {}", ex.what());
    }
}

void OscReceiver::processHeartbeat(const toio_control::osc::Message& message) {
    // /heartbeat: [deviceId (string), sequence (int32), seconds (int32), micros (int32), ...]
    if (message.arguments.size() < 4) {
        return;  // オプションなので警告なし
    }
    
    try {
        std::string deviceId;
        int sequence = 0;
        int seconds = 0;
        int micros = 0;
        
        if (std::holds_alternative<std::string>(message.arguments[0])) {
            deviceId = std::get<std::string>(message.arguments[0]);
        } else {
            return;
        }
        
        if (std::holds_alternative<std::int32_t>(message.arguments[1])) {
            sequence = std::get<std::int32_t>(message.arguments[1]);
        } else {
            return;
        }
        
        if (std::holds_alternative<std::int32_t>(message.arguments[2])) {
            seconds = std::get<std::int32_t>(message.arguments[2]);
        } else {
            return;
        }
        
        if (std::holds_alternative<std::int32_t>(message.arguments[3])) {
            micros = std::get<std::int32_t>(message.arguments[3]);
        } else {
            return;
        }
        
        // レイテンシを計算（簡易版、実際の時刻同期が必要）
        auto now = std::chrono::system_clock::now();
        auto nowSeconds = std::chrono::duration_cast<std::chrono::seconds>(
            now.time_since_epoch()).count();
        double latencyMs = (static_cast<double>(nowSeconds) - static_cast<double>(seconds)) * 1000.0;
        
        if (heartbeatCallback_) {
            heartbeatCallback_(deviceId, sequence, latencyMs);
        }
        
        spdlog::debug("Processed /heartbeat: deviceId={}, seq={}, latency={:.3f}ms", 
                     deviceId, sequence, latencyMs);
    } catch (const std::exception& ex) {
        spdlog::warn("Error processing /heartbeat: {}", ex.what());
    }
}

void OscReceiver::processPosition(const toio_control::osc::Message& message) {
    // OSCアドレスパターン: /toio/{deviceId}/position または /toio/position
    // 引数: x (float), y (float), angle (float), [deviceId (string)]
    
    if (message.arguments.size() < 3) {
        return;
    }
    
    ReceivedPosition pos;
    
    // デバイスIDを取得（アドレスから、または引数から）
    std::string address = message.address;
    if (address.find("/toio/") == 0 && address != "/toio/position") {
        // /toio/{deviceId}/position パターン
        size_t start = 6;  // "/toio/"の後
        size_t end = address.find("/", start);
        if (end != std::string::npos) {
            pos.deviceId = address.substr(start, end - start);
        } else {
            pos.deviceId = address.substr(start);
        }
    } else if (message.arguments.size() >= 4 && 
               std::holds_alternative<std::string>(message.arguments[3])) {
        // 引数にdeviceIdが含まれる
        pos.deviceId = std::get<std::string>(message.arguments[3]);
    } else {
        // デフォルト: アドレスから推測できない場合は"unknown"
        pos.deviceId = "unknown";
    }
    
    // 座標を取得
    if (std::holds_alternative<float>(message.arguments[0])) {
        pos.x = static_cast<double>(std::get<float>(message.arguments[0]));
    } else if (std::holds_alternative<std::int32_t>(message.arguments[0])) {
        pos.x = static_cast<double>(std::get<std::int32_t>(message.arguments[0]));
    } else {
        return;  // 無効な引数
    }
    
    if (std::holds_alternative<float>(message.arguments[1])) {
        pos.y = static_cast<double>(std::get<float>(message.arguments[1]));
    } else if (std::holds_alternative<std::int32_t>(message.arguments[1])) {
        pos.y = static_cast<double>(std::get<std::int32_t>(message.arguments[1]));
    } else {
        return;
    }
    
    if (std::holds_alternative<float>(message.arguments[2])) {
        pos.angle = static_cast<double>(std::get<float>(message.arguments[2]));
    } else if (std::holds_alternative<std::int32_t>(message.arguments[2])) {
        pos.angle = static_cast<double>(std::get<std::int32_t>(message.arguments[2]));
    } else {
        return;
    }
    
    pos.receivedTime = std::chrono::steady_clock::now();
    
    // コールバックを呼び出し
    if (positionCallback_) {
        positionCallback_(pos);
    }
    
    // 履歴に追加（オプション）
    {
        std::lock_guard<std::mutex> lock(positionsMutex_);
        receivedPositions_.push_back(pos);
        // 履歴を100件に制限
        if (receivedPositions_.size() > 100) {
            receivedPositions_.erase(receivedPositions_.begin());
        }
    }
}

std::vector<ReceivedPosition> OscReceiver::getReceivedPositions() {
    std::lock_guard<std::mutex> lock(positionsMutex_);
    return receivedPositions_;
}

}  // namespace swarm_control
