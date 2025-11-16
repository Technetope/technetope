#include <M5Unified.h>
#include <string>

#include "controller/toio_controller.h"
#include "ui/ui_helpers.h"

namespace {
constexpr uint32_t kScanDurationSec = 3;
constexpr uint32_t kRefreshIntervalMs = 1000;
constexpr char kTargetSuffix[] = "m7d";

ToioController g_toio;
UiHelpers g_ui;

void InitializeM5Hardware() {
  auto cfg = M5.config();
  cfg.clear_display = true;
  cfg.output_power = true;
  cfg.serial_baudrate = 115200;
  M5.begin(cfg);

  M5.Display.setRotation(3);
  g_ui.Begin();
  g_ui.DrawHeader("Scanning...");
}
}  // namespace

void PerformStartupTest() {
  constexpr uint8_t kLedR = 0x00;
  constexpr uint8_t kLedG = 0xff;
  constexpr uint8_t kLedB = 0x80;
  constexpr uint8_t kTestSpeed = 30;

  if (g_toio.setLedColor(kLedR, kLedG, kLedB)) {
    M5.Log.println("LED test applied.");
  }
  if (g_toio.driveMotor(kTestSpeed, kTestSpeed)) {
    delay(1000);
    g_toio.driveMotor(0, 0);
  }
}

void InitGoalFollowing() {
  float g_goalX = 300.0f;
  float g_goalY = 200.0f;
  
  g_toio.setGoalTuning(/*vmax=*/80.0f, /*wmax=*/70.0f, /*k_r=*/1.0f,
                       /*k_a=*/0.8f, /*reverse_threshold_deg=*/90.0f,
                       /*reverse_hysteresis_deg=*/10.0f);
  g_toio.setGoal(g_goalX, g_goalY, /*stop_distance=*/20.0f);
}

void setup() {
  InitializeM5Hardware();

  std::vector<std::string> scan_results;
  auto status = g_toio.scan(kScanDurationSec, &scan_results);
  g_ui.ShowInitResult(status);
  if (status != ToioController::InitStatus::kScanReady) {
    return;
  }
  g_ui.LogScanResults(scan_results);
  delay(1000);

  g_ui.DrawHeader("Connecting...");
  status = g_toio.connectBySuffix(kTargetSuffix);
  g_ui.ShowInitResult(status);
  if (status != ToioController::InitStatus::kConnected) {
    return;
  }

  const float board_voltage = M5.Power.getBatteryVoltage() *(3.3f/4096.0f);
  g_ui.UpdateStatus(g_toio.pose(), g_toio.hasPose(), g_toio.batteryLevel(),
                    g_toio.hasBatteryLevel(), board_voltage,
                    g_toio.ledColor(), g_toio.motorState(),
                    /*pose_dirty=*/true, /*battery_dirty=*/true,
                    kRefreshIntervalMs);
  g_toio.clearPoseDirty();
  g_toio.clearBatteryDirty();
  PerformStartupTest();
  InitGoalFollowing();
}

void loop() {
  M5.update();
  g_toio.loop();

  if (!g_toio.hasActiveCore()) {
    delay(100);
    return;
  }

  const bool pose_dirty = g_toio.poseDirty();
  const bool battery_dirty = g_toio.batteryDirty();

  const float board_voltage = M5.Power.getBatteryVoltage()*(3.3f/4096.0f);

  g_ui.UpdateStatus(g_toio.pose(), g_toio.hasPose(), g_toio.batteryLevel(),
                    g_toio.hasBatteryLevel(), board_voltage,
                    g_toio.ledColor(), g_toio.motorState(), pose_dirty,
                    battery_dirty, kRefreshIntervalMs);
  if (pose_dirty) {
    g_toio.clearPoseDirty();
  }
  if (battery_dirty) {
    g_toio.clearBatteryDirty();
  }

  delay(10);
}
