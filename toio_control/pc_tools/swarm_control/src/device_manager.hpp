#pragma once

#include <string>
#include <vector>
#include <map>
#include <set>
#include <optional>
#include <mutex>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <iomanip>
#include "robot_agent.hpp"
#include "toio_control/common/DeviceRegistry.h"

namespace swarm_control {

struct DeviceInfo {
    std::string deviceId;           // m5のデバイスID（DeviceRegistryから取得）
    std::string mac;                 // MACアドレス
    int assignedIndex;               // アサインされたインデックス（接続順、0始まり）
    int connectionOrder;            // 接続順（1始まり、表示用）
    SpeedCategory category;          // 速度カテゴリ
    bool isLowHomeostasis;          // 低い恒常性
    bool isAssigned;                 // アサイン完了フラグ
    std::chrono::steady_clock::time_point connectedTime;
    std::chrono::steady_clock::time_point assignedTime;
    double initialX, initialY;       // 初期位置
    double initialAngle;             // 初期角度
};

class DeviceManager {
public:
    // コンストラクタ: DeviceRegistryを参照
    DeviceManager(toio_control::common::DeviceRegistry* registry,
                 const std::filesystem::path& storagePath);
    
    // announceからデバイスを登録（DeviceRegistryを使用）
    std::optional<std::string> registerFromAnnounce(
        const std::string& mac,
        const std::string& firmwareVersion,
        std::optional<std::string> alias = std::nullopt);
    
    // 座標受信時にアサイン（動的アサイン）
    int assignFromPosition(const std::string& deviceId,
                          double x, double y, double angle);
    
    // デバイス情報の取得
    std::optional<DeviceInfo> getDeviceInfo(const std::string& deviceId) const;
    std::optional<DeviceInfo> getDeviceInfo(int assignedIndex) const;
    
    // 接続済みデバイス数
    size_t getConnectedCount() const;
    
    // アサイン済みデバイス数
    size_t getAssignedCount() const;
    
    // すべてのデバイス情報を取得
    std::vector<DeviceInfo> getAllDevices() const;
    
    // デバイスを削除（切断時、オプション）
    bool removeDevice(const std::string& deviceId);
    
    // 接続順とアサイン情報を表示
    void printAssignmentInfo() const;
    
    // 永続化
    void load();
    void save() const;

private:
    struct SwarmAssignment {
        int assignedIndex;
        int connectionOrder;
        SpeedCategory category;
        bool isLowHomeostasis;
        std::chrono::steady_clock::time_point assignedTime;
        double initialX, initialY;
        double initialAngle;
    };
    
    // 接続順に基づいてカテゴリを割り当て（現在の接続数に応じて割合的に）
    void assignCategory(SwarmAssignment& assignment, int connectionOrder);
    
    // 永続化用の内部メソッド
    void saveAssignments() const;
    void loadAssignments();
    
    toio_control::common::DeviceRegistry* registry_;
    std::map<std::string, SwarmAssignment> assignments_;  // deviceId -> assignment
    std::map<int, std::string> indexToDeviceId_;  // assignedIndex -> deviceId
    std::set<int> freeIndices_;  // 空きインデックス（削除されたインデックスを保持）
    int nextConnectionOrder_ = 1;
    std::filesystem::path storagePath_;
    mutable std::mutex mutex_;
};

}  // namespace swarm_control
