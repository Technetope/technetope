#pragma once

#include <string>
#include <vector>

#include "../controller/toio_controller.h"

class CommandDispatcher {
 public:
  explicit CommandDispatcher(ToioController& controller);

  struct ScanResult {
    ToioController::InitStatus status = ToioController::InitStatus::kInvalidArgument;
    std::vector<std::string> suffixes;
  };

  ScanResult Scan(uint32_t duration_sec);
  ToioController::InitStatus Connect(const std::string& suffix);

  bool SetLed(uint8_t r, uint8_t g, uint8_t b);
  bool DriveMotor(int8_t left_speed, int8_t right_speed);

  void SetGoal(float x, float y, float stop_distance);
  void ClearGoal();

  void SetStatusSubscription(bool enable);
  bool StatusSubscriptionEnabled() const { return status_subscription_enabled_; }

  struct StatusSnapshot {
    bool has_core = false;
    bool has_pose = false;
    CubePose pose{};
    bool has_battery = false;
    uint8_t battery_level = 0;
    ToioLedColor led{};
    ToioMotorState motor{};
    bool goal_active = false;
  };
  StatusSnapshot GetStatus() const;

 private:
  ToioController& controller_;
  bool status_subscription_enabled_ = false;
};
