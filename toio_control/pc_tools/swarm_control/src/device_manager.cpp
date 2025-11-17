#include "device_manager.hpp"
#include "params.hpp"
#include "json.hpp"  // nlohmann/json
#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <spdlog/spdlog.h>

#if defined(_WIN32)
#include <time.h>
#else
#include <time.h>
#endif

namespace swarm_control {

namespace {
using json = nlohmann::json;

std::string timePointToIso(std::chrono::steady_clock::time_point tp) {
    (void)tp;  // steady_clockを直接ISO化しない
    // steady_clockはepoch timeを持たないので、system_clockに変換できない
    // 代わりに、経過時間を文字列化するか、system_clockを使用
    auto now = std::chrono::system_clock::now();
    auto tt = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::gmtime(&tt);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

std::chrono::steady_clock::time_point isoToTimePoint(const std::string& iso) {
    // ISO文字列からtime_pointに変換（簡易版）
    // 実際にはsystem_clockを使用し、steady_clockへの変換は近似
    std::tm tm{};
    std::istringstream iss(iso);
    iss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    if (iss.fail()) {
        return std::chrono::steady_clock::now();
    }
#if defined(_WIN32)
    auto timeT = _mkgmtime(&tm);
#else
    auto timeT = timegm(&tm);
#endif
    (void)timeT;  // 現状は近似として現在時刻を返す
    // system_clockからsteady_clockへの変換は近似（現在時刻を使用）
    return std::chrono::steady_clock::now();
}

std::string categoryToString(SpeedCategory category) {
    switch (category) {
        case SpeedCategory::FAST: return "FAST";
        case SpeedCategory::MODERATE: return "MODERATE";
        case SpeedCategory::SLOW: return "SLOW";
    }
    return "UNKNOWN";
}

SpeedCategory stringToCategory(const std::string& str) {
    if (str == "FAST") return SpeedCategory::FAST;
    if (str == "MODERATE") return SpeedCategory::MODERATE;
    if (str == "SLOW") return SpeedCategory::SLOW;
    return SpeedCategory::MODERATE;  // デフォルト
}

}  // namespace

DeviceManager::DeviceManager(toio_control::common::DeviceRegistry* registry,
                             const std::filesystem::path& storagePath)
    : registry_(registry),
      assignments_(),
      indexToDeviceId_(),
      freeIndices_(),
      nextConnectionOrder_(1),
      storagePath_(storagePath) {
    if (!registry_) {
        throw std::invalid_argument("DeviceRegistry cannot be null");
    }
    load();
}

std::optional<std::string> DeviceManager::registerFromAnnounce(
    const std::string& mac,
    const std::string& firmwareVersion,
    std::optional<std::string> alias) {
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        auto now = std::chrono::system_clock::now();
        auto deviceState = registry_->registerAnnounce(mac, firmwareVersion, alias, now);
        
        spdlog::info("Device registered from announce: {} (MAC: {})", 
                     deviceState.id, mac);
        
        return deviceState.id;
    } catch (const std::exception& ex) {
        spdlog::error("Failed to register device from announce: {}", ex.what());
        return std::nullopt;
    }
}

int DeviceManager::assignFromPosition(const std::string& deviceId,
                                     double x, double y, double angle) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 既にアサイン済みかチェック
    auto it = assignments_.find(deviceId);
    if (it != assignments_.end()) {
        return it->second.assignedIndex;  // 既にアサイン済み
    }
    
    // DeviceRegistryでデバイスが登録されているか確認
    auto deviceState = registry_->findById(deviceId);
    if (!deviceState.has_value()) {
        spdlog::warn("Device not found in registry: {}", deviceId);
        return -1;  // デバイス未登録
    }
    
    // 新しいアサインを作成
    SwarmAssignment assignment;
    // 空きインデックスがあれば再利用、なければ新規インデックスを割り当て
    if (!freeIndices_.empty()) {
        assignment.assignedIndex = *freeIndices_.begin();  // 最小の空きインデックスを使用
        freeIndices_.erase(freeIndices_.begin());
    } else {
        assignment.assignedIndex = static_cast<int>(assignments_.size());
    }
    assignment.connectionOrder = nextConnectionOrder_++;
    assignment.assignedTime = std::chrono::steady_clock::now();
    assignment.initialX = x;
    assignment.initialY = y;
    assignment.initialAngle = angle;
    
    // 接続順に基づいてカテゴリを割り当て
    assignCategory(assignment, assignment.connectionOrder);
    
    assignments_[deviceId] = assignment;
    indexToDeviceId_[assignment.assignedIndex] = deviceId;
    
    // 永続化
    saveAssignments();
    
    spdlog::info("Device assigned: {} -> index {}, order {}, category {}", 
                 deviceId, assignment.assignedIndex, 
                 assignment.connectionOrder, 
                 categoryToString(assignment.category));
    
    return assignment.assignedIndex;
}

void DeviceManager::assignCategory(SwarmAssignment& assignment, int connectionOrder) {
    // 現在の接続数（接続順 = 現在の接続数）
    const int currentCount = connectionOrder;
    
    // 割合的に計算（切り上げ）
    const int fastCount = static_cast<int>(std::ceil(currentCount * 0.2));
    const int moderateCount = static_cast<int>(std::ceil(currentCount * 0.6));
    
    if (connectionOrder <= fastCount) {
        assignment.category = SpeedCategory::FAST;
        assignment.isLowHomeostasis = false;
    } else if (connectionOrder <= fastCount + moderateCount) {
        assignment.category = SpeedCategory::MODERATE;
        assignment.isLowHomeostasis = false;
    } else {
        assignment.category = SpeedCategory::SLOW;
        assignment.isLowHomeostasis = true;  // 最後の20%は低い恒常性
    }
}

std::optional<DeviceInfo> DeviceManager::getDeviceInfo(const std::string& deviceId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // DeviceRegistryから基本情報を取得
    auto deviceState = registry_->findById(deviceId);
    if (!deviceState.has_value()) {
        return std::nullopt;
    }
    
    // SwarmAssignmentからアサイン情報を取得
    auto it = assignments_.find(deviceId);
    if (it == assignments_.end()) {
        return std::nullopt;
    }
    
    const auto& assignment = it->second;
    DeviceInfo info;
    info.deviceId = deviceId;
    info.mac = deviceState->mac;
    info.assignedIndex = assignment.assignedIndex;
    info.connectionOrder = assignment.connectionOrder;
    info.category = assignment.category;
    info.isLowHomeostasis = assignment.isLowHomeostasis;
    info.isAssigned = true;
    info.assignedTime = assignment.assignedTime;
    info.initialX = assignment.initialX;
    info.initialY = assignment.initialY;
    info.initialAngle = assignment.initialAngle;
    
    return info;
}

std::optional<DeviceInfo> DeviceManager::getDeviceInfo(int assignedIndex) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = indexToDeviceId_.find(assignedIndex);
    if (it == indexToDeviceId_.end()) {
        return std::nullopt;
    }
    
    return getDeviceInfo(it->second);
}

size_t DeviceManager::getConnectedCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return assignments_.size();
}

size_t DeviceManager::getAssignedCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return assignments_.size();
}

std::vector<DeviceInfo> DeviceManager::getAllDevices() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<DeviceInfo> devices;
    for (const auto& [deviceId, assignment] : assignments_) {
        auto deviceState = registry_->findById(deviceId);
        if (deviceState.has_value()) {
            DeviceInfo info;
            info.deviceId = deviceId;
            info.mac = deviceState->mac;
            info.assignedIndex = assignment.assignedIndex;
            info.connectionOrder = assignment.connectionOrder;
            info.category = assignment.category;
            info.isLowHomeostasis = assignment.isLowHomeostasis;
            info.isAssigned = true;
            info.assignedTime = assignment.assignedTime;
            info.initialX = assignment.initialX;
            info.initialY = assignment.initialY;
            info.initialAngle = assignment.initialAngle;
            devices.push_back(info);
        }
    }
    
    return devices;
}

bool DeviceManager::removeDevice(const std::string& deviceId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = assignments_.find(deviceId);
    if (it == assignments_.end()) {
        return false;
    }
    
    int assignedIndex = it->second.assignedIndex;
    assignments_.erase(it);
    indexToDeviceId_.erase(assignedIndex);
    
    // 削除したインデックスを空きインデックスとして保持
    freeIndices_.insert(assignedIndex);
    
    saveAssignments();
    return true;
}

void DeviceManager::printAssignmentInfo() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::cout << "\n========================================\n";
    std::cout << "Device Assignment Information\n";
    std::cout << "========================================\n";
    std::cout << std::left 
              << std::setw(18) << "Connection Order"
              << std::setw(20) << "Device ID"
              << std::setw(18) << "Assigned Index"
              << std::setw(12) << "Category"
              << std::setw(18) << "Low Homeostasis"
              << "\n";
    std::cout << std::string(90, '-') << "\n";
    
    for (const auto& [deviceId, assignment] : assignments_) {
        auto deviceState = registry_->findById(deviceId);
        std::string displayId = deviceState.has_value() ? deviceId : deviceId + " (not in registry)";
        
        std::cout << std::left
                  << std::setw(18) << assignment.connectionOrder
                  << std::setw(20) << displayId
                  << std::setw(18) << assignment.assignedIndex
                  << std::setw(12) << categoryToString(assignment.category)
                  << std::setw(18) << (assignment.isLowHomeostasis ? "Yes" : "No")
                  << "\n";
    }
    
    std::cout << "========================================\n";
    std::cout << "Total Assigned: " << assignments_.size() << " devices\n";
    std::cout << "========================================\n\n";
}

void DeviceManager::load() {
    loadAssignments();
}

void DeviceManager::save() const {
    saveAssignments();
}

void DeviceManager::saveAssignments() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        // ディレクトリを作成
        auto parentPath = storagePath_.parent_path();
        if (!parentPath.empty() && !std::filesystem::exists(parentPath)) {
            std::filesystem::create_directories(parentPath);
        }
        
        json root = json::array();
        for (const auto& [deviceId, assignment] : assignments_) {
            json node;
            node["device_id"] = deviceId;
            node["assigned_index"] = assignment.assignedIndex;
            node["connection_order"] = assignment.connectionOrder;
            node["category"] = categoryToString(assignment.category);
            node["is_low_homeostasis"] = assignment.isLowHomeostasis;
            node["assigned_time"] = timePointToIso(assignment.assignedTime);
            node["initial_x"] = assignment.initialX;
            node["initial_y"] = assignment.initialY;
            node["initial_angle"] = assignment.initialAngle;
            root.push_back(node);
        }
        
        std::ofstream output(storagePath_);
        if (!output) {
            spdlog::error("Failed to open assignment storage file: {}", storagePath_.string());
            return;
        }
        
        output << root.dump(2);
        spdlog::debug("Saved {} assignments to {}", assignments_.size(), storagePath_.string());
    } catch (const std::exception& ex) {
        spdlog::error("Failed to save assignments: {}", ex.what());
    }
}

void DeviceManager::loadAssignments() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    assignments_.clear();
    indexToDeviceId_.clear();
    freeIndices_.clear();
    nextConnectionOrder_ = 1;
    
    if (!std::filesystem::exists(storagePath_)) {
        spdlog::info("Assignment storage file not found: {}", storagePath_.string());
        return;
    }
    
    try {
        std::ifstream input(storagePath_);
        if (!input) {
            spdlog::warn("Failed to open assignment storage file: {}", storagePath_.string());
            return;
        }
        
        json root;
        input >> root;
        
        if (!root.is_array()) {
            spdlog::warn("Invalid assignment storage format: expected array");
            return;
        }
        
        int maxConnectionOrder = 0;
        for (const auto& entry : root) {
            std::string deviceId = entry.at("device_id").get<std::string>();
            
            // DeviceRegistryでデバイスが存在するか確認
            auto deviceState = registry_->findById(deviceId);
            if (!deviceState.has_value()) {
                spdlog::warn("Device {} not found in registry, skipping assignment", deviceId);
                continue;
            }
            
            SwarmAssignment assignment;
            assignment.assignedIndex = entry.at("assigned_index").get<int>();
            assignment.connectionOrder = entry.at("connection_order").get<int>();
            assignment.category = stringToCategory(entry.at("category").get<std::string>());
            assignment.isLowHomeostasis = entry.value("is_low_homeostasis", false);
            assignment.assignedTime = isoToTimePoint(entry.value("assigned_time", ""));
            assignment.initialX = entry.value("initial_x", 0.0);
            assignment.initialY = entry.value("initial_y", 0.0);
            assignment.initialAngle = entry.value("initial_angle", 0.0);
            
            assignments_[deviceId] = assignment;
            indexToDeviceId_[assignment.assignedIndex] = deviceId;
            
            if (assignment.connectionOrder > maxConnectionOrder) {
                maxConnectionOrder = assignment.connectionOrder;
            }
        }
        
        nextConnectionOrder_ = maxConnectionOrder + 1;
        spdlog::info("Loaded {} assignments from {}", assignments_.size(), storagePath_.string());
    } catch (const std::exception& ex) {
        spdlog::error("Failed to load assignments: {}", ex.what());
        assignments_.clear();
        indexToDeviceId_.clear();
    }
}

}  // namespace swarm_control
