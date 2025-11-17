#include "controller/toio_controller.h"

#include <cmath>

namespace {
std::string ExtractToioSuffix(const std::string& name) {
  constexpr char kPrefix[] = "toio-";
  if (name.rfind(kPrefix, 0) == 0 && name.size() > sizeof(kPrefix) - 1) {
    return name.substr(sizeof(kPrefix) - 1);
  }
  return name;
}
}  // namespace

ToioController::InitStatus ToioController::scan(
    uint32_t scan_duration_sec, std::vector<std::string>* out_suffixes) {
  if (!out_suffixes) {
    return InitStatus::kInvalidArgument;
  }

  scan_duration_sec_ = scan_duration_sec;
  last_scan_results_.clear();
  out_suffixes->clear();

  auto cores = toio_.scan(scan_duration_sec_);
  if (cores.empty()) {
    return InitStatus::kNoCubeFound;
  }

  last_scan_results_.reserve(cores.size());
  out_suffixes->reserve(cores.size());

  for (auto* core : cores) {
    ScanEntry entry;
    entry.core = core;
    entry.suffix = ExtractToioSuffix(core->getName());
    last_scan_results_.push_back(entry);
    out_suffixes->push_back(entry.suffix);
  }

  return InitStatus::kScanReady;
}

ToioController::InitStatus ToioController::connectBySuffix(
    const std::string& suffix) {
  if (last_scan_results_.empty()) {
    return InitStatus::kTargetNotFound;
  }

  ToioCore* target = nullptr;
  if (suffix.empty()) {
    target = last_scan_results_.front().core;
  } else {
    for (const auto& entry : last_scan_results_) {
      if (entry.suffix == suffix) {
        target = entry.core;
        break;
      }
    }
  }

  if (!target) {
    return InitStatus::kTargetNotFound;
  }
  InitStatus status = connectCore(target);
  if (status != InitStatus::kConnected) {
    return status;
  }
  configureCore(target);
  return InitStatus::kConnected;
}

void ToioController::loop() {
  toio_.loop();
  updateGoalTracking();
}

bool ToioController::setLedColor(uint8_t r, uint8_t g, uint8_t b) {
  if (!active_core_) {
    return false;
  }
  active_core_->turnOnLed(r, g, b);
  led_color_.r = r;
  led_color_.g = g;
  led_color_.b = b;
  return true;
}

namespace {
uint8_t ClampSpeed(int value) {
  if (value < 0) {
    value = -value;
  }
  if (value > 100) {
    value = 100;
  }
  return static_cast<uint8_t>(value);
}
}  // namespace

bool ToioController::driveMotor(int8_t left_speed, int8_t right_speed) {
  if (!active_core_) {
    return false;
  }
  const bool left_dir = left_speed >= 0;
  const bool right_dir = right_speed >= 0;
  const uint8_t left_mag = ClampSpeed(left_speed);
  const uint8_t right_mag = ClampSpeed(right_speed);
  active_core_->controlMotor(left_dir, left_mag, right_dir, right_mag);
  motor_state_.left_speed = left_speed; // clamp していない生の値を保存
  motor_state_.right_speed = right_speed; // clamp していない生の値を保存
  return true;
}

void ToioController::setGoal(float x, float y, float stop_distance) {
  goal_tracker_.setGoal(x, y, stop_distance);
}

void ToioController::clearGoal() {
  goal_tracker_.clearGoal();
  driveMotor(0, 0);
}

void ToioController::setGoalTuning(float vmax, float wmax, float k_r,
                                   float k_a, float reverse_threshold_deg,
                                   float reverse_hysteresis_deg) {
  goal_tracker_.setTuning(vmax, wmax, k_r, k_a, reverse_threshold_deg,
                          reverse_hysteresis_deg);
}

ToioController::InitStatus ToioController::connectCore(ToioCore* core) {
  if (!core) {
    return InitStatus::kInvalidArgument;
  }
  if (!core->connect()) {
    return InitStatus::kConnectionFailed;
  }
  return InitStatus::kConnected;
}

void ToioController::configureCore(ToioCore* core) {
  core->setIDnotificationSettings(/*minimum_interval=*/5, /*condition=*/0x01); 
  // 5*10ms=50msごとにID通知、condition=0x01で位置変化時に通知
  core->setIDmissedNotificationSettings(/*sensitivity=*/10);
  // 10*10ms=100ms間IDが読めなかったら通知
  core->onIDReaderData([this](ToioCoreIDData data) { handleIdData(data); });
  core->onBattery([this](uint8_t level) { handleBatteryLevel(level); });

  active_core_ = core;
  handleBatteryLevel(core->getBatteryLevel());
  handleIdData(core->getIDReaderData());
}

void ToioController::handleIdData(const ToioCoreIDData& data) {
  if (data.type == ToioCoreIDTypePosition) {
    pose_.x = data.position.cubePosX;
    pose_.y = data.position.cubePosY;
    pose_.angle = data.position.cubeAngleDegree;
    pose_.on_mat = true;
    has_pose_ = true;
  } else if (data.type == ToioCoreIDTypeNone) {
    pose_.on_mat = false;
    has_pose_ = true;
  } else {
    has_pose_ = false;
  }
  pose_dirty_ = true;
  pose_updated_ms_ = millis();
}

void ToioController::handleBatteryLevel(uint8_t level) {
  battery_level_ = level;
  has_battery_ = true;
  battery_dirty_ = true;
  battery_updated_ms_ = millis();
}

void ToioController::updateGoalTracking() {
  if (!goal_tracker_.hasGoal() || !active_core_ || !has_pose_) {
    return;
  }

  int8_t left_speed = 0;
  int8_t right_speed = 0;
  if (goal_tracker_.computeCommand(pose_, &left_speed, &right_speed)) {
    driveMotor(left_speed, right_speed);
  }
}
