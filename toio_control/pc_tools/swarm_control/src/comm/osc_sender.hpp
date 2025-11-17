#pragma once

#include <string>
#include <vector>
#include <memory>
#include <asio.hpp>
#include "toio_control/osc/OscTransport.h"
#include "toio_control/osc/OscPacket.h"

namespace swarm_control {

struct TargetPosition {
    int robotIndex = 0;
    double x = 0.0;        // 目標位置 X (mm)
    double y = 0.0;       // 目標位置 Y (mm)
    double angle = 0.0;   // 目標角度 (rad)
};

/**
 * @brief OSC送信クラス（toioドライブ制御用）
 * 
 * このクラスはtoioロボットのリアルタイム制御に特化したOSC送信実装です。
 * 特徴:
 * - 高頻度の連続送信に最適化（1秒ごとなど）
 * - 常時接続を維持（接続の確立・切断のオーバーヘッドを回避）
 * - 複数ロボットの目標座標をバンドルで一括送信
 * 
 * 注意: scheduler（音響制御）とは異なり、常時接続が必要です。
 */
class OscSender {
public:
    // コンストラクタ（送信先アドレスとポートを指定）
    OscSender(const std::string& address = "127.0.0.1", 
              int port = 5005);
    
    // 単一のロボットの目標座標を送信
    void sendTarget(int robotIndex, double x, double y, double angle);
    
    // 複数のロボットの目標座標を一括送信（バンドル）
    void sendTargets(const std::vector<TargetPosition>& targets);
    
    // 暗号化を有効化
    void enableEncryption(const toio_control::osc::OscEncryptor::Key256& key,
                         const toio_control::osc::OscEncryptor::Iv128& iv);
    void disableEncryption();
    bool encryptionEnabled() const;
    
    // 接続状態をチェック
    bool isConnected() const { return connected_; }

private:
    // OSCメッセージを送信
    void sendMessage(const toio_control::osc::Message& message);
    
    // OSCバンドルを送信
    void sendBundle(const toio_control::osc::Bundle& bundle);
    
    std::unique_ptr<toio_control::osc::IoContextRunner> ioContextRunner_;
    std::unique_ptr<toio_control::osc::OscSender> sender_;
    std::string address_;
    [[maybe_unused]] int port_;
    bool connected_ = false;
    
    // OSCアドレスのパターン
    std::string getTargetAddress(int robotIndex) const {
        return "/toio/" + std::to_string(robotIndex) + "/target";
    }
};

}  // namespace swarm_control
