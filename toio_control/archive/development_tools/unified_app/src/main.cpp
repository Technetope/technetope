#include "unified_config.hpp"
#include "toio_control/common/DeviceRegistry.h"
#include "toio_control/scheduler/SchedulerController.h"
#include "toio_control/osc/OscTransport.h"
#include "toio_control/osc/OscEncryptor.h"

// Swarm control components
#include "../swarm_control/src/device_manager.hpp"
#include "../swarm_control/src/robot_agent.hpp"
#include "../swarm_control/src/spatial_density_grid.hpp"
#include "../swarm_control/src/cluster_detector.hpp"
#include "../swarm_control/src/predictive_avoidance.hpp"
#include "../swarm_control/src/spot_potential.hpp"
#include "../swarm_control/src/urgent_escape.hpp"
#include "../swarm_control/src/boid_model.hpp"
#include "../swarm_control/src/osc_receiver.hpp"
#include "../swarm_control/src/osc_sender.hpp"
#include "../swarm_control/src/params.hpp"
#include "../swarm_control/src/human_spot.hpp"
#include "../swarm_control/src/velocity.hpp"

#include <CLI11.hpp>
#include <spdlog/spdlog.h>
#include <iostream>
#include <memory>
#include <vector>
#include <mutex>
#include <map>
#include <thread>
#include <chrono>
#include <algorithm>
#include <filesystem>

using namespace toio_control::unified;
using namespace swarm_control;

int main(int argc, char** argv) {
    CLI::App app{"Toio Control - Unified Swarm & Sound Control"};
    
    std::filesystem::path configPath = "config/toio_control_config.json";
    app.add_option("-c,--config", configPath, "Configuration file path")
        ->check(CLI::ExistingFile);
    
    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& e) {
        return app.exit(e);
    }
    
    // 設定を読み込む
    auto config = loadConfig(configPath);
    
    // パラメータを初期化（swarm用）
    params::initializeParams();
    
    // DeviceRegistryを初期化
    toio_control::common::DeviceRegistry deviceRegistry(config.swarm.deviceRegistry);
    try {
        deviceRegistry.load();
        spdlog::info("DeviceRegistry loaded from {}", config.swarm.deviceRegistry.string());
    } catch (const std::exception& ex) {
        spdlog::warn("Failed to load DeviceRegistry: {}", ex.what());
    }
    
    // DeviceManagerを初期化（swarm用）
    DeviceManager deviceManager(&deviceRegistry, config.swarm.assignmentStorage);
    try {
        deviceManager.load();
        spdlog::info("DeviceManager loaded from {}", config.swarm.assignmentStorage.string());
    } catch (const std::exception& ex) {
        spdlog::warn("Failed to load DeviceManager: {}", ex.what());
    }
    
    // 統一OSC送信器を初期化
    std::unique_ptr<OscSender> unifiedOscSender;
    if (config.swarm.enabled || config.scheduler.enabled) {
        unifiedOscSender = std::make_unique<OscSender>(
            config.osc.sendAddress, config.osc.sendPort);
        
        if (config.osc.encryptionEnabled && config.osc.key.has_value() && config.osc.iv.has_value()) {
            unifiedOscSender->enableEncryption(config.osc.key.value(), config.osc.iv.value());
            spdlog::info("OSC encryption enabled");
        }
    }
    
    // Swarm control components
    std::vector<std::unique_ptr<RobotAgent>> robots;
    std::mutex robotsMutex;
    std::map<int, std::chrono::steady_clock::time_point> robotLastUpdateTime;
    std::mutex updateTimeMutex;
    SpatialDensityGrid densityGrid;
    ClusterDetector clusterDetector;
    PredictiveAvoidance predictiveAvoidance;
    SpotPotentialField potentialField;
    UrgentEscape urgentEscape;
    BoidModel boidModel;
    
    // OSC受信器を初期化（swarm用）
    std::unique_ptr<OscReceiver> oscReceiver;
    if (config.swarm.enabled) {
        oscReceiver = std::make_unique<OscReceiver>(config.osc.listenPort);
        
        if (config.osc.encryptionEnabled && config.osc.key.has_value() && config.osc.iv.has_value()) {
            oscReceiver->enableEncryption(config.osc.key.value(), config.osc.iv.value());
        }
        
        // announceコールバック
        oscReceiver->setAnnounceCallback([&](const std::string& deviceId,
                                              const std::string& mac,
                                              const std::string& fwVersion) {
            auto registeredId = deviceManager.registerFromAnnounce(mac, fwVersion);
            if (registeredId) {
                spdlog::info("Device registered: {} (MAC: {})", *registeredId, mac);
            }
        });
        
        // 座標コールバック
        oscReceiver->setPositionCallback([&](const ReceivedPosition& pos) {
            int assignedIndex = deviceManager.assignFromPosition(
                pos.deviceId, pos.x, pos.y, pos.angle);
            
            if (assignedIndex >= 0) {
                auto deviceInfo = deviceManager.getDeviceInfo(assignedIndex);
                if (deviceInfo.has_value()) {
                    bool isNewRobot = false;
                    {
                        std::lock_guard<std::mutex> lock(robotsMutex);
                        bool exists = false;
                        for (const auto& robot : robots) {
                            if (robot && robot->getIndex() == assignedIndex) {
                                exists = true;
                                break;
                            }
                        }
                        
                        if (!exists) {
                            auto robot = std::make_unique<RobotAgent>(
                                pos.x, pos.y, assignedIndex,
                                deviceInfo->category,
                                deviceInfo->isLowHomeostasis);
                            robot->setClusterDetector(&clusterDetector);
                            robots.push_back(std::move(robot));
                            isNewRobot = true;
                        }
                    }
                    
                    {
                        std::lock_guard<std::mutex> lock(updateTimeMutex);
                        robotLastUpdateTime[assignedIndex] = pos.receivedTime;
                    }
                    
                    if (isNewRobot) {
                        spdlog::info("New robot added: index={}, deviceId={}", assignedIndex, pos.deviceId);
                    }
                }
            }
        });
        
        oscReceiver->start();
    }
    
    // Scheduler controller
    std::unique_ptr<toio_control::scheduler::SchedulerController> schedulerController;
    std::thread schedulerThread;
    bool schedulerRunning = false;
    
    if (config.scheduler.enabled && config.scheduler.timelinePath.has_value()) {
        schedulerController = std::make_unique<toio_control::scheduler::SchedulerController>();
        
        toio_control::scheduler::SchedulerConfig schedulerConfig;
        schedulerConfig.timelinePath = config.scheduler.timelinePath.value();
        schedulerConfig.host = config.osc.sendAddress;
        schedulerConfig.port = config.osc.sendPort;
        schedulerConfig.leadTimeOverride = config.scheduler.leadTime;
        schedulerConfig.bundleSpacing = config.scheduler.bundleSpacing;
        schedulerConfig.broadcast = config.osc.broadcast;
        schedulerConfig.targetMapPath = config.scheduler.targetMapPath;
        schedulerConfig.defaultTargets = config.scheduler.defaultTargets;
        schedulerConfig.encryptOsc = config.osc.encryptionEnabled;
        if (config.osc.key.has_value() && config.osc.iv.has_value()) {
            schedulerConfig.oscKey = config.osc.key.value();
            schedulerConfig.oscIv = config.osc.iv.value();
        }
        
        // Schedulerを別スレッドで実行
        schedulerRunning = true;
        schedulerThread = std::thread([&]() {
            try {
                auto report = schedulerController->execute(schedulerConfig);
                spdlog::info("Scheduler completed: {} bundles sent", report.bundles.size());
            } catch (const std::exception& ex) {
                spdlog::error("Scheduler error: {}", ex.what());
            }
            schedulerRunning = false;
        });
    }
    
    // メインループ（swarm制御）
    auto lastUpdate = std::chrono::steady_clock::now();
    auto lastInfoPrint = std::chrono::steady_clock::now();
    const double updateInterval = 1.0;
    const double infoPrintInterval = 10.0;
    
    spdlog::info("Toio Control started");
    if (config.swarm.enabled) {
        spdlog::info("Swarm control: enabled");
        spdlog::info("OSC receiver: port {}", config.osc.listenPort);
    }
    if (config.scheduler.enabled) {
        spdlog::info("Scheduler: enabled");
        if (config.scheduler.timelinePath.has_value()) {
            spdlog::info("Timeline: {}", config.scheduler.timelinePath->string());
        }
    }
    spdlog::info("OSC sender: {}:{}", config.osc.sendAddress, config.osc.sendPort);
    
    while (true) {
        auto now = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(now - lastUpdate).count();
        
        if (elapsed >= updateInterval && config.swarm.enabled) {
            // デバイス切断検知
            {
                std::lock_guard<std::mutex> lock(updateTimeMutex);
                std::lock_guard<std::mutex> lockRobots(robotsMutex);
                
                const double DEVICE_TIMEOUT_SECONDS = 5.0;
                auto timeoutThreshold = now - std::chrono::seconds(static_cast<int>(DEVICE_TIMEOUT_SECONDS));
                std::vector<int> devicesToRemove;
                
                for (auto it = robotLastUpdateTime.begin(); it != robotLastUpdateTime.end();) {
                    if (it->second < timeoutThreshold) {
                        devicesToRemove.push_back(it->first);
                        it = robotLastUpdateTime.erase(it);
                    } else {
                        ++it;
                    }
                }
                
                for (int index : devicesToRemove) {
                    auto deviceInfo = deviceManager.getDeviceInfo(index);
                    if (deviceInfo.has_value()) {
                        deviceManager.removeDevice(deviceInfo->deviceId);
                        spdlog::info("Device disconnected: {} (index {})", deviceInfo->deviceId, index);
                    }
                    
                    robots.erase(
                        std::remove_if(robots.begin(), robots.end(),
                            [index](const std::unique_ptr<RobotAgent>& robot) {
                                return robot && robot->getIndex() == index;
                            }),
                        robots.end()
                    );
                }
            }
            
            // ロボット更新
            std::vector<RobotAgent*> robotPtrs;
            {
                std::lock_guard<std::mutex> lock(robotsMutex);
                for (const auto& robot : robots) {
                    if (robot) {
                        robotPtrs.push_back(robot.get());
                    }
                }
            }
            
            if (!robotPtrs.empty()) {
                // 人間の位置情報（テスト用）
                std::vector<HumanSpot> humanSpots;
                HumanSpot spot;
                spot.headX = 500.0;
                spot.headY = 500.0;
                spot.velocityX = 0.0;
                spot.velocityY = 0.0;
                spot.urgent = false;
                humanSpots.push_back(spot);
                
                // ポテンシャル場を更新
                potentialField.updateSpots(humanSpots, robotPtrs);
                densityGrid.update(robotPtrs);
                urgentEscape.checkUrgentEscape(robotPtrs, humanSpots);
                
                // 各ロボットの状態を更新
                const double dt = updateInterval;
                for (auto* robot : robotPtrs) {
                    if (!robot) continue;
                    robot->updateAutonomy(dt, humanSpots, robotPtrs);
                    robot->applyCollisionAvoidance(robotPtrs, dt, &densityGrid, &predictiveAvoidance);
                    robot->applyCollisionBrake(robotPtrs);
                    robot->applyBoundaryReflection();
                    robot->applySpeedLimit();
                    robot->clampPosition();
                    robot->update(dt);
                }
                
                // 目標座標を送信
                std::vector<TargetPosition> targets;
                for (auto* robot : robotPtrs) {
                    if (!robot) continue;
                    auto pos = robot->getPosition();
                    TargetPosition target;
                    target.robotIndex = robot->getIndex();
                    target.x = pos.x;
                    target.y = pos.y;
                    target.angle = robot->getAngle();
                    targets.push_back(target);
                }
                
                if (unifiedOscSender) {
                    unifiedOscSender->sendTargets(targets);
                }
            }
            
            // 定期的に情報を表示
            double infoElapsed = std::chrono::duration<double>(now - lastInfoPrint).count();
            if (infoElapsed >= infoPrintInterval) {
                if (config.swarm.enabled) {
                    deviceManager.printAssignmentInfo();
                }
                lastInfoPrint = now;
            }
            
            // 定期的に永続化
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
        
        // Schedulerが終了したかチェック
        if (config.scheduler.enabled && !schedulerRunning && schedulerThread.joinable()) {
            schedulerThread.join();
            spdlog::info("Scheduler thread finished");
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    return 0;
}

