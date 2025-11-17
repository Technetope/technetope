#include "protocol/protocol_handler.h"

#include <ArduinoJson.h>
#include <M5Unified.h>
#include <cstring>

namespace {
constexpr uint32_t kDefaultScanDurationSec = 3;

// Utility to read optional string field.
const char* ReadString(const JsonVariantConst& value) {
  if (value.is<const char*>()) {
    return value.as<const char*>();
  }
  return nullptr;
}
}  // namespace

ProtocolHandler::ProtocolHandler(CommandDispatcher& commands,
                                 SendCallback sender)
    : commands_(commands), send_(std::move(sender)) {}

void ProtocolHandler::HandleClientConnected() {
  connected_ = true;
  commands_.SetStatusSubscription(false);
  SendHello();
}

void ProtocolHandler::HandleClientDisconnected() {
  connected_ = false;
  commands_.SetStatusSubscription(false);
}

void ProtocolHandler::HandleMessage(const std::string& payload) {
  StaticJsonDocument<512> doc;
  const auto err = deserializeJson(doc, payload);
  const char* id = doc["id"];
  if (err) {
    SendError(id, "json-parse-failed");
    return;
  }
  const char* type = doc["type"];
  if (!type) {
    SendError(id, "missing-type");
    return;
  }

  if (strcmp(type, "scan") == 0) {
    auto result = commands_.Scan(kDefaultScanDurationSec);
    SendScanResult(id, result);
    return;
  }

  if (strcmp(type, "connect") == 0) {
    const char* suffix = ReadString(doc["suffix"]);
    std::string suffix_str = suffix ? suffix : "";
    auto status = commands_.Connect(suffix_str);
    SendConnectResult(id, suffix_str, status);
    return;
  }

  if (strcmp(type, "led") == 0) {
    int r = doc["r"] | -1;
    int g = doc["g"] | -1;
    int b = doc["b"] | -1;
    if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255) {
      SendError(id, "invalid-led");
      return;
    }
    bool ok = commands_.SetLed(static_cast<uint8_t>(r),
                               static_cast<uint8_t>(g),
                               static_cast<uint8_t>(b));
    SendAck("led-result", id, ok, ok ? nullptr : "no-active-core");
    return;
  }

  if (strcmp(type, "motor") == 0) {
    int left = doc["left"] | -101;
    int right = doc["right"] | -101;
    if (left < -100 || left > 100 || right < -100 || right > 100) {
      SendError(id, "invalid-motor");
      return;
    }
    bool ok = commands_.DriveMotor(static_cast<int8_t>(left),
                                   static_cast<int8_t>(right));
    SendAck("motor-result", id, ok, ok ? nullptr : "no-active-core");
    return;
  }

  if (strcmp(type, "goal-set") == 0) {
    if (!doc["x"].is<float>() || !doc["y"].is<float>()) {
      SendError(id, "invalid-goal");
      return;
    }
    const float x = doc["x"];
    const float y = doc["y"];
    const float stop = doc["stop_distance"] | 20.0f;
    commands_.SetGoal(x, y, stop);
    SendAck("goal-set-result", id, true);
    return;
  }

  if (strcmp(type, "goal-clear") == 0) {
    commands_.ClearGoal();
    SendAck("goal-clear-result", id, true);
    return;
  }

  if (strcmp(type, "status-request") == 0) {
    SendStatus(id);
    return;
  }

  if (strcmp(type, "status-subscribe") == 0) {
    if (!doc["enable"].is<bool>() && !doc["enable"].is<int>()) {
      SendError(id, "invalid-subscribe");
      return;
    }
    const bool enable = doc["enable"];
    commands_.SetStatusSubscription(enable);
    SendAck("status-subscribe-result", id, true);
    return;
  }

  SendError(id, "unknown-type");
}

void ProtocolHandler::MaybeSendStatus(bool pose_dirty, bool battery_dirty) {
  if (!connected_) {
    return;
  }
  if (!commands_.StatusSubscriptionEnabled()) {
    return;
  }
  if (pose_dirty || battery_dirty) {
    SendStatus(nullptr);
  }
}

void ProtocolHandler::SendHello() {
  StaticJsonDocument<96> doc;
  doc["type"] = "hello";
  std::string payload;
  serializeJson(doc, payload);
  send_(payload);
}

void ProtocolHandler::SendScanResult(
    const char* id, const CommandDispatcher::ScanResult& result) {
  StaticJsonDocument<384> doc;
  doc["type"] = "scan-result";
  if (id) doc["id"] = id;
  doc["status"] = static_cast<int>(result.status);
  doc["count"] = result.suffixes.size();
  auto array = doc.createNestedArray("suffixes");
  for (const auto& suffix : result.suffixes) {
    array.add(suffix);
  }

  std::string payload;
  serializeJson(doc, payload);
  send_(payload);
}

void ProtocolHandler::SendConnectResult(const char* id,
                                        const std::string& suffix,
                                        ToioController::InitStatus status) {
  StaticJsonDocument<192> doc;
  doc["type"] = "connect-result";
  if (id) doc["id"] = id;
  doc["suffix"] = suffix;
  doc["status"] = MapConnectStatus(status);

  std::string payload;
  serializeJson(doc, payload);
  send_(payload);
}

void ProtocolHandler::SendStatus(const char* id) {
  auto snapshot = commands_.GetStatus();

  StaticJsonDocument<320> doc;
  doc["type"] = "status";
  if (id) doc["id"] = id;
  doc["connected"] = snapshot.has_core;

  if (snapshot.has_pose) {
    doc["x"] = snapshot.pose.x;
    doc["y"] = snapshot.pose.y;
    doc["angle"] = snapshot.pose.angle;
    doc["on_mat"] = snapshot.pose.on_mat ? 1 : 0;
  }
  if (snapshot.has_battery) {
    doc["batt"] = snapshot.battery_level;
  }
  doc["goal_active"] = snapshot.goal_active;

  auto led = doc.createNestedArray("led");
  led.add(snapshot.led.r);
  led.add(snapshot.led.g);
  led.add(snapshot.led.b);

  auto motor = doc.createNestedArray("motor");
  motor.add(snapshot.motor.left_speed);
  motor.add(snapshot.motor.right_speed);

  std::string payload;
  serializeJson(doc, payload);
  send_(payload);
}

void ProtocolHandler::SendAck(const char* type, const char* id, bool success,
                              const char* message) {
  StaticJsonDocument<160> doc;
  doc["type"] = type;
  if (id) doc["id"] = id;
  doc["ok"] = success;
  if (message) {
    doc["message"] = message;
  }
  std::string payload;
  serializeJson(doc, payload);
  send_(payload);
}

void ProtocolHandler::SendError(const char* id, const char* message) {
  StaticJsonDocument<160> doc;
  doc["type"] = "error";
  if (id) doc["id"] = id;
  doc["message"] = message;

  std::string payload;
  serializeJson(doc, payload);
  send_(payload);
}

const char* ProtocolHandler::MapConnectStatus(ToioController::InitStatus status) {
  switch (status) {
    case ToioController::InitStatus::kConnected:
      return "connected";
    case ToioController::InitStatus::kTargetNotFound:
      return "not_found";
    case ToioController::InitStatus::kConnectionFailed:
      return "failed";
    case ToioController::InitStatus::kNoCubeFound:
      return "no_scan";
    default:
      return "invalid";
  }
}
