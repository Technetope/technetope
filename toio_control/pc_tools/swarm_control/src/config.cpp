#include "config.hpp"
#include <fstream>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <iomanip>
#include <spdlog/spdlog.h>

namespace swarm_control {
namespace config {

namespace {
using json = nlohmann::json;

// 16進数文字列をバイト配列に変換（toio_control/pc_tools/scheduler/src/main.cppから参考）
template<std::size_t N>
std::array<std::uint8_t, N> parseHexBytes(const std::string& text) {
    std::string sanitized = text;
    sanitized.erase(std::remove_if(sanitized.begin(), sanitized.end(),
                    [](char c) { return std::isspace(static_cast<unsigned char>(c)); }),
                    sanitized.end());
    
    if (sanitized.size() >= 2 &&
        sanitized[0] == '0' &&
        (sanitized[1] == 'x' || sanitized[1] == 'X')) {
        sanitized.erase(0, 2);
    }
    
    if (sanitized.size() != N * 2) {
        throw std::runtime_error("Expected " + std::to_string(N * 2) + " hex characters, got " + std::to_string(sanitized.size()));
    }
    
    std::array<std::uint8_t, N> bytes{};
    for (std::size_t i = 0; i < N; ++i) {
        std::string byteStr = sanitized.substr(i * 2, 2);
        if (!std::isxdigit(static_cast<unsigned char>(byteStr[0])) ||
            !std::isxdigit(static_cast<unsigned char>(byteStr[1]))) {
            throw std::runtime_error("Invalid hex characters in key/iv");
        }
        bytes[i] = static_cast<std::uint8_t>(std::stoul(byteStr, nullptr, 16));
    }
    return bytes;
}
}

Config getDefaultConfig() {
    Config config;
    return config;
}

Config loadConfig(const std::filesystem::path& configPath) {
    Config config = getDefaultConfig();
    
    if (!std::filesystem::exists(configPath)) {
        spdlog::info("Config file not found: {}, using defaults", configPath.string());
        return config;
    }
    
    try {
        std::ifstream input(configPath);
        if (!input) {
            spdlog::warn("Failed to open config file: {}, using defaults", configPath.string());
            return config;
        }
        
        json root;
        input >> root;
        
        // OSC設定
        if (root.contains("osc")) {
            const auto& osc = root["osc"];
            if (osc.contains("listen_port")) {
                config.osc.listenPort = osc["listen_port"].get<int>();
            }
            if (osc.contains("send_port")) {
                config.osc.sendPort = osc["send_port"].get<int>();
            }
            if (osc.contains("send_address")) {
                config.osc.sendAddress = osc["send_address"].get<std::string>();
            }
            if (osc.contains("encryption")) {
                const auto& encryption = osc["encryption"];
                if (encryption.contains("enabled")) {
                    config.osc.encryptionEnabled = encryption["enabled"].get<bool>();
                }
                if (encryption.contains("key_file")) {
                    config.osc.encryptionKeyFile = encryption["key_file"].get<std::string>();
                    
                    // 暗号化が有効な場合、キーファイルからキーとIVを読み込む
                    if (config.osc.encryptionEnabled) {
                        try {
                            std::filesystem::path keyFilePath = config.osc.encryptionKeyFile.value();
                            if (!std::filesystem::exists(keyFilePath)) {
                                spdlog::warn("Encryption key file not found: {}", keyFilePath.string());
                            } else {
                                std::ifstream keyInput(keyFilePath);
                                if (!keyInput) {
                                    spdlog::warn("Failed to open encryption key file: {}", keyFilePath.string());
                                } else {
                                    json keyRoot;
                                    keyInput >> keyRoot;
                                    
                                    if (keyRoot.contains("osc")) {
                                        const auto& oscNode = keyRoot.at("osc");
                                        const auto keyIt = oscNode.find("key_hex");
                                        const auto ivIt = oscNode.find("iv_hex");
                                        
                                        if (keyIt != oscNode.end() && keyIt->is_string() &&
                                            ivIt != oscNode.end() && ivIt->is_string()) {
                                            config.osc.key = parseHexBytes<32>(keyIt->get<std::string>());
                                            config.osc.iv = parseHexBytes<16>(ivIt->get<std::string>());
                                            spdlog::info("Encryption keys loaded from {}", keyFilePath.string());
                                        } else {
                                            spdlog::warn("Encryption key file missing key_hex or iv_hex: {}", keyFilePath.string());
                                        }
                                    } else {
                                        spdlog::warn("Encryption key file missing 'osc' object: {}", keyFilePath.string());
                                    }
                                }
                            }
                        } catch (const std::exception& ex) {
                            spdlog::error("Failed to load encryption keys from {}: {}", 
                                         config.osc.encryptionKeyFile->string(), ex.what());
                        }
                    }
                }
            }
        }
        
        // Swarm設定
        if (root.contains("swarm")) {
            const auto& swarm = root["swarm"];
            if (swarm.contains("assignment_storage")) {
                config.swarm.assignmentStorage = swarm["assignment_storage"].get<std::string>();
            }
            if (swarm.contains("device_registry")) {
                config.swarm.deviceRegistry = swarm["device_registry"].get<std::string>();
            }
        }
        
        spdlog::info("Config loaded from {}", configPath.string());
    } catch (const std::exception& ex) {
        spdlog::error("Failed to load config: {}, using defaults", ex.what());
    }
    
    return config;
}

}  // namespace config
}  // namespace swarm_control

