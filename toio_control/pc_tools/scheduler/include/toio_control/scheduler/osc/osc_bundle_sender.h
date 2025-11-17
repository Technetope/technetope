#pragma once

#include "toio_control/osc/OscEncryptor.h"
#include "toio_control/osc/OscPacket.h"

#include <memory>
#include <string>
#include <vector>

namespace toio_control::osc {
class IoContextRunner;
class OscSender;
}  // namespace toio_control::osc

namespace toio_control::scheduler::osc {

/**
 * @brief OSCバンドル送信クラス（音響制御用）
 * 
 * このクラスは音響タイムラインのスケジューリングに特化したOSC送信実装です。
 * 特徴:
 * - 低頻度のバッチ送信に最適化
 * - 送信完了後は接続を切断可能（リソース効率化）
 * - タイムラインに基づいた送信間隔制御
 * 
 * 注意: swarm_control（toioドライブ制御）とは異なり、常時接続を維持する必要はありません。
 */
class OscBundleSender {
public:
    OscBundleSender(const std::string& host, std::uint16_t port, bool broadcast = true);
    ~OscBundleSender();
    
    // 暗号化設定
    void enableEncryption(const toio_control::osc::OscEncryptor::Key256& key,
                         const toio_control::osc::OscEncryptor::Iv128& iv);
    void disableEncryption();
    bool encryptionEnabled() const;
    
    // バンドル送信
    void sendBundle(const toio_control::osc::Bundle& bundle);
    void sendBundles(const std::vector<toio_control::osc::Bundle>& bundles,
                     double bundleSpacing = 0.01);
    
    // 接続管理（送信完了後に明示的に切断可能）
    bool isConnected() const;
    void disconnect();  // 明示的に接続を切断（リソース解放）

private:
    std::unique_ptr<toio_control::osc::IoContextRunner> ioContextRunner_;
    std::unique_ptr<toio_control::osc::OscSender> sender_;
    std::string host_;
    std::uint16_t port_;
    bool broadcast_;
    bool connected_ = false;
    bool encryptionEnabled_ = false;
};

}  // namespace toio_control::scheduler::osc

