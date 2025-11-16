#pragma once

#include <stdint.h>
#include <string>
#include <vector>

#include "controller/toio_controller.h"

class UiHelpers {
 public:
  void Begin();
  void DrawHeader(const char* message);
  void ShowInitResult(ToioController::InitStatus status);
   // スキャン結果（suffix 一覧）のログ＋画面出力
  void LogScanResults(const std::vector<std::string>& suffixes);
  void UpdateStatus(const CubePose& pose, bool has_pose, uint8_t battery_level,
                    bool has_battery, float board_voltage,
                    const ToioLedColor& led, const ToioMotorState& motor,
                    bool pose_dirty, bool battery_dirty,
                    uint32_t refresh_interval_ms);

 private:
  struct UiStatus {
    CubePose pose{};
    bool has_pose = false;
    uint8_t battery_level = 0;
    bool has_battery = false;
    float board_voltage = 0.0f;
    ToioLedColor led{};
    ToioMotorState motor{};
  };

  void ShowStatus(uint32_t now_ms);

  UiStatus status_{};
  uint32_t last_display_ms_ = 0;
};
