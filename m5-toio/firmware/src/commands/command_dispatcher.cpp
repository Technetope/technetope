#include "commands/command_dispatcher.h"

CommandDispatcher::CommandDispatcher(ToioController& controller)
    : controller_(controller) {}

CommandDispatcher::ScanResult CommandDispatcher::Scan(uint32_t duration_sec) {
  ScanResult result;
  result.status = controller_.scan(duration_sec, &result.suffixes);
  return result;
}

ToioController::InitStatus CommandDispatcher::Connect(
    const std::string& suffix) {
  return controller_.connectBySuffix(suffix);
}

bool CommandDispatcher::SetLed(uint8_t r, uint8_t g, uint8_t b) {
  return controller_.setLedColor(r, g, b);
}

bool CommandDispatcher::DriveMotor(int8_t left_speed, int8_t right_speed) {
  return controller_.driveMotor(left_speed, right_speed);
}

void CommandDispatcher::SetGoal(float x, float y, float stop_distance) {
  controller_.setGoal(x, y, stop_distance);
}

void CommandDispatcher::ClearGoal() {
  controller_.clearGoal();
}

void CommandDispatcher::SetStatusSubscription(bool enable) {
  status_subscription_enabled_ = enable;
}

CommandDispatcher::StatusSnapshot CommandDispatcher::GetStatus() const {
  StatusSnapshot snapshot;
  snapshot.has_core = controller_.hasActiveCore();
  snapshot.has_pose = controller_.hasPose();
  snapshot.pose = controller_.pose();
  snapshot.has_battery = controller_.hasBatteryLevel();
  snapshot.battery_level = controller_.batteryLevel();
  snapshot.led = controller_.ledColor();
  snapshot.motor = controller_.motorState();
  snapshot.goal_active = controller_.hasGoal();
  return snapshot;
}
