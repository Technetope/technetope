#pragma once

#include <string>
#include <filesystem>
#include <optional>
#include <array>
#include <cstdint>
#include <vector>
#include "json.hpp"
#include "toio_control/osc/OscEncryptor.h"

namespace toio_control {
namespace unified {

struct OscConfig {
    int listenPort = 5006;
    int sendPort = 5005;
    std::string sendAddress = "255.255.255.255";
    bool broadcast = true;
    bool encryptionEnabled = false;
    std::optional<std::filesystem::path> encryptionKeyFile;
    std::optional<toio_control::osc::OscEncryptor::Key256> key;
    std::optional<toio_control::osc::OscEncryptor::Iv128> iv;
};

struct SwarmConfig {
    std::filesystem::path assignmentStorage = "state/swarm_assignments.json";
    std::filesystem::path deviceRegistry = "state/devices.json";
    bool enabled = true;
};

struct SchedulerConfig {
    std::optional<std::filesystem::path> timelinePath;
    double leadTime = 3.0;
    double bundleSpacing = 0.01;
    std::filesystem::path targetMapPath;
    std::vector<std::string> defaultTargets;
    bool enabled = false;
};

struct UnifiedConfig {
    OscConfig osc;
    SwarmConfig swarm;
    SchedulerConfig scheduler;
};

// 設定ファイルを読み込む
UnifiedConfig loadConfig(const std::filesystem::path& configPath = "config/toio_control_config.json");

// デフォルト設定を返す
UnifiedConfig getDefaultConfig();

}  // namespace unified
}  // namespace toio_control

