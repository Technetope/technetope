#include <M5Unified.h>
#include <cstdio>
#include <string>

#include "commands/command_dispatcher.h"
#include "controller/toio_controller.h"
#include "net/websocket_server.h"
#include "protocol/protocol_handler.h"
#include "ui/ui_helpers.h"

namespace {
constexpr uint32_t kRefreshIntervalMs = 1000;
constexpr uint16_t kWebsocketPort = 9000;

// Wi-Fi credentials: replace with your network settings.
constexpr char kWifiSsid[] = "TomoshibiTechnology_IoT";
constexpr char kWifiPassword[] = "All_outlook";

ToioController g_toio;
UiHelpers g_ui;
WebsocketServer g_server;
CommandDispatcher g_commands(g_toio);
ProtocolHandler g_protocol(
    g_commands, [](const std::string& payload) { return g_server.Send(payload); });

void InitializeM5Hardware() {
  auto cfg = M5.config();
  cfg.clear_display = true;
  cfg.output_power = true;
  cfg.serial_baudrate = 115200;
  M5.begin(cfg);

  M5.Display.setRotation(3);
  g_ui.Begin();
  g_ui.DrawHeader("Wi-Fi connecting...");
}
}  // namespace

void setup() {
  InitializeM5Hardware();

  const bool net_ok = g_server.Begin(
      kWifiSsid, kWifiPassword, kWebsocketPort,
      [](const std::string& message) { g_protocol.HandleMessage(message); },
      []() { g_protocol.HandleClientConnected(); },
      []() { g_protocol.HandleClientDisconnected(); });
  if (!net_ok) {
    g_ui.DrawHeader("Wi-Fi failed");
    return;
  }

  char header[64];
  snprintf(header, sizeof(header), "WS %s:%u",
           g_server.local_ip().toString().c_str(),
           static_cast<unsigned>(kWebsocketPort));
  g_ui.DrawHeader(header);
}

void loop() {
  M5.update();
  g_toio.loop();
  g_server.Loop();

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

  g_protocol.MaybeSendStatus(pose_dirty, battery_dirty);

  // delay(10);
}
