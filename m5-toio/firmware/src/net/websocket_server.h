#pragma once

#include <Arduino.h>
#include <ArduinoWebsockets.h>
#include <WiFi.h>

#include <functional>
#include <string>

class WebsocketServer {
 public:
  using MessageHandler = std::function<void(const std::string&)>;
  using ConnectHandler = std::function<void()>;
  using DisconnectHandler = std::function<void()>;

  bool Begin(const char* ssid, const char* password, uint16_t port,
             MessageHandler on_message, ConnectHandler on_connect,
             DisconnectHandler on_disconnect);
  void Loop();

  bool Send(const std::string& message);
  bool Connected() const { return has_client_; }
  IPAddress local_ip() const { return WiFi.localIP(); }

 private:
  void HandleNewClient(websockets::WebsocketsClient&& client);

  websockets::WebsocketsServer server_;
  websockets::WebsocketsClient client_;
  bool has_client_ = false;

  MessageHandler on_message_;
  ConnectHandler on_connect_;
  DisconnectHandler on_disconnect_;
};
