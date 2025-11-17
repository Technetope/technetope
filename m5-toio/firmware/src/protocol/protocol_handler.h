#pragma once

#include <functional>
#include <string>

#include "../commands/command_dispatcher.h"

class ProtocolHandler {
 public:
  using SendCallback = std::function<bool(const std::string&)>;

  ProtocolHandler(CommandDispatcher& commands, SendCallback sender);

  void HandleClientConnected();
  void HandleClientDisconnected();
  void HandleMessage(const std::string& payload);
  void MaybeSendStatus(bool pose_dirty, bool battery_dirty);

 private:
  void SendHello();
  void SendScanResult(const char* id,
                      const CommandDispatcher::ScanResult& result);
  void SendConnectResult(const char* id, const std::string& suffix,
                         ToioController::InitStatus status);
  void SendStatus(const char* id = nullptr);
  void SendAck(const char* type, const char* id, bool success,
               const char* message = nullptr);
  void SendError(const char* id, const char* message);
  const char* MapConnectStatus(ToioController::InitStatus status);

  CommandDispatcher& commands_;
  SendCallback send_;
  bool connected_ = false;
};
