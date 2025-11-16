#pragma once

#include <string>
#include <filesystem>
#include <optional>
#include <array>
#include <cstdint>
#include "json.hpp"
#include "acoustics/osc/OscEncryptor.h"

namespace swarm_control {
namespace config {

struct OscConfig {
    int listenPort = 5006;
    int sendPort = 5005;
    std::string sendAddress = "127.0.0.1";
    bool encryptionEnabled = false;
    std::optional<std::filesystem::path> encryptionKeyFile;
    std::optional<acoustics::osc::OscEncryptor::Key256> key;
    std::optional<acoustics::osc::OscEncryptor::Iv128> iv;
};

struct SwarmConfig {
    std::filesystem::path assignmentStorage = "state/swarm_assignments.json";
    std::filesystem::path deviceRegistry = "state/devices.json";
};

struct Config {
    OscConfig osc;
    SwarmConfig swarm;
};

// 設定ファイルを読み込む
Config loadConfig(const std::filesystem::path& configPath = "config/swarm_config.json");

// デフォルト設定を返す
Config getDefaultConfig();

}  // namespace config
}  // namespace swarm_control

