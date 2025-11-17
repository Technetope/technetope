#include <iostream>
#include <vector>
#include <memory>
#include <chrono>
#include <thread>
#include <random>
#include <cmath>
#include "config/params.hpp"
#include "algorithm/agent/robot_agent.hpp"
#include "algorithm/spatial/spatial_density_grid.hpp"
#include "algorithm/flocking/cluster_detector.hpp"
#include "algorithm/collision/predictive_avoidance.hpp"
#include "algorithm/spatial/spot_potential.hpp"
#include "algorithm/collision/urgent_escape.hpp"
#include "algorithm/flocking/boid_model.hpp"
#include "utils/types/velocity.hpp"
#include "comm/osc_sender.hpp"
#include "comm/osc_receiver.hpp"
#include "utils/device/device_manager.hpp"
#include "utils/types/human_spot.hpp"
#include "utils/utils.hpp"
#include "toio_control/common/DeviceRegistry.h"
#include "config/config.hpp"
#include <mutex>
#include <map>
#include <algorithm>
#include <filesystem>
#include <spdlog/spdlog.h>

using namespace swarm_control;

// 人間の位置情報を更新（テスト用）
void updateHumanSpots(std::vector<HumanSpot>& humanSpots) {
    // テスト用に1つのスポットを配置
    humanSpots.clear();
    HumanSpot spot;
    spot.headX = 500.0;  // mm
    spot.headY = 500.0;  // mm
    spot.velocityX = 0.0;
    spot.velocityY = 0.0;
    spot.urgent = false;
    humanSpots.push_back(spot);
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    // パラメータを初期化
    params::initializeParams();
    
    // 設定ファイルを読み込む
    auto config = config::loadConfig();
    
    // DeviceRegistryを初期化
    toio_control::common::DeviceRegistry deviceRegistry(config.swarm.deviceRegistry);
    try {
        deviceRegistry.load();
        spdlog::info("DeviceRegistry loaded from {}", config.swarm.deviceRegistry.string());
    } catch (const std::exception& ex) {
        spdlog::warn("Failed to load DeviceRegistry: {}", ex.what());
    }
    
    // DeviceManagerを初期化（DeviceRegistryを参照）
    DeviceManager deviceManager(&deviceRegistry, config.swarm.assignmentStorage);
    try {
        deviceManager.load();
        spdlog::info("DeviceManager loaded from {}", config.swarm.assignmentStorage.string());
    } catch (const std::exception& ex) {
        spdlog::warn("Failed to load DeviceManager: {}", ex.what());
    }
    
    // コンポーネントを初期化
    std::vector<std::unique_ptr<RobotAgent>> robots;
    std::mutex robotsMutex;  // ロボットリストの保護用
    std::map<int, std::chrono::steady_clock::time_point> robotLastUpdateTime;  // ロボットの最後の更新時刻
    std::mutex updateTimeMutex;  // 更新時刻マップの保護用
    SpatialDensityGrid densityGrid;
    ClusterDetector clusterDetector;
    PredictiveAvoidance predictiveAvoidance;
    SpotPotentialField potentialField;
    UrgentEscape urgentEscape;
    BoidModel boidModel;
    OscSender oscSender(config.osc.sendAddress, config.osc.sendPort);
    
    // OSC受信リスナーを初期化
    OscReceiver oscReceiver(config.osc.listenPort);
    
    // 暗号化を有効化（設定ファイルから読み込む）
    if (config.osc.encryptionEnabled && config.osc.key.has_value() && config.osc.iv.has_value()) {
        oscSender.enableEncryption(config.osc.key.value(), config.osc.iv.value());
        oscReceiver.enableEncryption(config.osc.key.value(), config.osc.iv.value());
        spdlog::info("Encryption enabled with keys from config");
    } else if (config.osc.encryptionEnabled) {
        spdlog::warn("Encryption enabled but keys not loaded from key file");
    }
    
    // announceコールバックを設定
    oscReceiver.setAnnounceCallback([&](const std::string& deviceId,
                                        const std::string& mac,
                                        const std::string& fwVersion) {
        auto registeredId = deviceManager.registerFromAnnounce(mac, fwVersion);
        if (registeredId) {
            spdlog::info("Device registered from announce: {} (MAC: {})", *registeredId, mac);
        } else {
            spdlog::warn("Failed to register device from announce: {} (MAC: {})", deviceId, mac);
        }
    });
    
    // heartbeatコールバックを設定（オプション）
    oscReceiver.setHeartbeatCallback([&](const std::string& deviceId,
                                         int sequence,
                                         double latencyMs) {
        spdlog::debug("Heartbeat received: deviceId={}, seq={}, latency={:.3f}ms", 
                     deviceId, sequence, latencyMs);
        // DeviceRegistryに記録（将来の拡張用）
    });
    
    // 座標コールバックを設定
    oscReceiver.setPositionCallback([&](const ReceivedPosition& pos) {
        // アサイン（動的アサイン）
        int assignedIndex = deviceManager.assignFromPosition(
            pos.deviceId, pos.x, pos.y, pos.angle);
        
        if (assignedIndex >= 0) {
            // デバイス情報を取得
            auto deviceInfo = deviceManager.getDeviceInfo(assignedIndex);
            
            if (deviceInfo.has_value()) {
                bool isNewRobot = false;
                {
                    std::lock_guard<std::mutex> lock(robotsMutex);
                    // 既存のロボットをチェック
                    bool exists = false;
                    for (const auto& robot : robots) {
                        if (robot && robot->getIndex() == assignedIndex) {
                            exists = true;
                            break;
                        }
                    }
                    
                    if (!exists) {
                        // ロボットエージェントを作成（接続順に基づいてアサイン）
                        auto robot = std::make_unique<RobotAgent>(
                            pos.x, pos.y, assignedIndex,
                            deviceInfo->category,
                            deviceInfo->isLowHomeostasis);
                        robot->setClusterDetector(&clusterDetector);
                        robots.push_back(std::move(robot));
                        isNewRobot = true;
                    }
                }
                
                // 更新時刻を記録
                {
                    std::lock_guard<std::mutex> lock(updateTimeMutex);
                    robotLastUpdateTime[assignedIndex] = pos.receivedTime;
                }
                
                if (isNewRobot) {
                    // アサイン情報を表示
                    std::cout << "\n=== Device Assigned ===\n";
                    deviceManager.printAssignmentInfo();
                    std::cout << "========================\n\n";
                }
            }
        } else {
            spdlog::warn("Failed to assign device: {} (not registered?)", pos.deviceId);
        }
    });
    
    // OSC受信リスナーを開始
    oscReceiver.start();
    
    // 人間の位置情報
    std::vector<HumanSpot> humanSpots;
    
    // メインループ（1秒ごと）
    auto lastUpdate = std::chrono::steady_clock::now();
    auto lastInfoPrint = std::chrono::steady_clock::now();
    const double updateInterval = 1.0;  // 1秒
    const double infoPrintInterval = 10.0;  // 10秒ごとに情報を表示
    
    spdlog::info("Swarm control started");
    spdlog::info("OSC receiver listening on port {}", config.osc.listenPort);
    spdlog::info("OSC sender sending to {}:{}", config.osc.sendAddress, config.osc.sendPort);
    spdlog::info("Sending OSC messages every {} seconds", updateInterval);
    std::cout << "\nSwarm control started. Waiting for devices to connect...\n";
    std::cout << "OSC receiver listening on port " << config.osc.listenPort << "\n";
    std::cout << "OSC sender sending to " << config.osc.sendAddress << ":" << config.osc.sendPort << "\n";
    std::cout << "Sending OSC messages every " << updateInterval << " seconds.\n\n";
    
    while (true) {
        auto now = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(now - lastUpdate).count();
        
        if (elapsed >= updateInterval) {
            // 人間の位置情報を更新
            updateHumanSpots(humanSpots);
            
            // 時間ステップ
            const double dt = updateInterval;
            
            // デバイス切断検知と削除処理
            {
                std::lock_guard<std::mutex> lock(updateTimeMutex);
                std::lock_guard<std::mutex> lockRobots(robotsMutex);
                
                const double DEVICE_TIMEOUT_SECONDS = 5.0;  // 5秒間更新がなければタイムアウト
                auto timeoutThreshold = now - std::chrono::seconds(static_cast<int>(DEVICE_TIMEOUT_SECONDS));
                std::vector<int> devicesToRemove;
                
                for (auto it = robotLastUpdateTime.begin(); it != robotLastUpdateTime.end();) {
                    if (it->second < timeoutThreshold) {
                        // タイムアウト：デバイスを削除
                        devicesToRemove.push_back(it->first);
                        it = robotLastUpdateTime.erase(it);
                    } else {
                        ++it;
                    }
                }
                
                // タイムアウトしたデバイスを削除
                for (int index : devicesToRemove) {
                    // DeviceManagerから削除
                    auto deviceInfo = deviceManager.getDeviceInfo(index);
                    if (deviceInfo.has_value()) {
                        deviceManager.removeDevice(deviceInfo->deviceId);
                        spdlog::info("Device disconnected (timeout): {} (index {})", 
                                    deviceInfo->deviceId, index);
                    }
                    
                    // ロボットリストから削除
                    robots.erase(
                        std::remove_if(robots.begin(), robots.end(),
                            [index](const std::unique_ptr<RobotAgent>& robot) {
                                return robot && robot->getIndex() == index;
                            }),
                        robots.end()
                    );
                }
            }
            
            // ロボットリストを保護して取得（nullptrチェック付き）
            std::vector<RobotAgent*> robotPtrs;
            {
                std::lock_guard<std::mutex> lock(robotsMutex);
                for (const auto& robot : robots) {
                    if (robot) {  // nullptrチェック
                        robotPtrs.push_back(robot.get());
                    }
                }
            }
            
            // アサイン済みのロボットがいる場合のみ更新
            if (!robotPtrs.empty()) {
                // ポテンシャル場を更新
                potentialField.updateSpots(humanSpots, robotPtrs);
                
                // 多層混雑度推定を更新
                densityGrid.update(robotPtrs);
                
                // 緊急退避をチェック
                urgentEscape.checkUrgentEscape(robotPtrs, humanSpots);
                
                // 各ロボットの状態を更新
                std::vector<Velocity> robotStates(robotPtrs.size());
                
                for (size_t i = 0; i < robotPtrs.size(); ++i) {
                    auto* robot = robotPtrs[i];
                    
                    // nullptrチェック
                    if (!robot) continue;
                    
                    // 自律性を更新
                    robot->updateAutonomy(dt, humanSpots, robotPtrs);
                    
                    // 衝突回避と境界処理
                    robot->applyCollisionAvoidance(robotPtrs, dt, 
                                                  &densityGrid, 
                                                  &predictiveAvoidance);
                    robot->applyCollisionBrake(robotPtrs);
                    robot->applyBoundaryReflection();
                    robot->applySpeedLimit();
                    robot->clampPosition();
                    
                    // 位置を更新
                    robot->update(dt);
                    
                    // 状態を保存
                    auto vel = robot->getVelocity();
                    robotStates[i].vx = vel.vx;
                    robotStates[i].vy = vel.vy;
                }
                
                // 群れ行動を適用（オプション）
                // boidModel.applyFlocking(robotPtrs, robotStates, dt);
                
                // 各ロボットの目標座標を計算してOSC送信
                std::vector<TargetPosition> targets;
                for (auto* robot : robotPtrs) {
                    if (!robot) continue;  // nullptrチェック
                    
                    auto pos = robot->getPosition();
                    
                    TargetPosition target;
                    target.robotIndex = robot->getIndex();
                    target.x = pos.x;
                    target.y = pos.y;
                    target.angle = robot->getAngle();
                    targets.push_back(target);
                }
                
                // OSCで一括送信（バンドル）
                oscSender.sendTargets(targets);
                
                // ログ出力（DEBUGレベル）
                spdlog::debug("Updated {} robots, sent OSC bundle", robotPtrs.size());
            } else {
                // ロボットが接続されていない場合
                spdlog::debug("Waiting for devices to connect... (assigned: {})", 
                             deviceManager.getAssignedCount());
            }
            
            // 定期的にアサイン情報を表示（10秒ごと）
            double infoElapsed = std::chrono::duration<double>(now - lastInfoPrint).count();
            if (infoElapsed >= infoPrintInterval) {
                deviceManager.printAssignmentInfo();
                lastInfoPrint = now;
            }
            
            // 定期的に永続化（30秒ごと）
            static auto lastSave = std::chrono::steady_clock::now();
            double saveElapsed = std::chrono::duration<double>(now - lastSave).count();
            if (saveElapsed >= 30.0) {
                try {
                    deviceManager.save();
                    deviceRegistry.save();
                    lastSave = now;
                } catch (const std::exception& ex) {
                    spdlog::error("Failed to save state: {}", ex.what());
                }
            }
            
            lastUpdate = now;
        }
        
        // CPU使用率を下げるために少し待機
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    return 0;
}
