#include "net/websocket_server.h"

#include <M5Unified.h>
#include <utility>

namespace {
constexpr uint32_t kWifiConnectTimeoutMs = 15000;
constexpr uint32_t kWifiRetryDelayMs = 250;
}  // namespace

bool WebsocketServer::Begin(const char* ssid, const char* password,
                            uint16_t port, MessageHandler on_message,
                            ConnectHandler on_connect,
                            DisconnectHandler on_disconnect) {
  on_message_ = std::move(on_message);
  on_connect_ = std::move(on_connect);
  on_disconnect_ = std::move(on_disconnect);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  const uint32_t start_ms = millis();
  while (WiFi.status() != WL_CONNECTED &&
         millis() - start_ms < kWifiConnectTimeoutMs) {
    delay(kWifiRetryDelayMs);
  }
  if (WiFi.status() != WL_CONNECTED) {
    M5.Log.println("Wi-Fi connection failed.");
    return false;
  }

  server_.listen(port);

  M5.Log.printf("Wi-Fi connected: %s\n",
                WiFi.localIP().toString().c_str());
  M5.Log.printf("WebSocket listening on port %u\n",
                static_cast<unsigned>(port));
  return true;
}

void WebsocketServer::Loop() {
  if (!has_client_) {
    auto maybe_client = server_.accept();
    if (maybe_client.available()) {
      HandleNewClient(std::move(maybe_client));
    }
  }

  if (has_client_) {
    client_.poll();
    if (!client_.available()) {
      has_client_ = false;
      if (on_disconnect_) {
        on_disconnect_();
      }
    }
  }
}

bool WebsocketServer::Send(const std::string& message) {
  if (!has_client_ || !client_.available()) {
    return false;
  }
  return client_.send(message.c_str(), message.size());
}

void WebsocketServer::HandleNewClient(websockets::WebsocketsClient&& client) {
  client_ = std::move(client);
  has_client_ = true;

  client_.onMessage([this](websockets::WebsocketsMessage msg) {
    if (on_message_ && msg.isText()) {
      const std::string payload = msg.data().c_str();
      on_message_(payload);
    }
  });

  client_.onEvent([this](websockets::WebsocketsEvent event, String data) {
    if (event == websockets::WebsocketsEvent::ConnectionClosed) {
      has_client_ = false;
      if (on_disconnect_) {
        on_disconnect_();
      }
    } else {
      M5.Log.printf("WebSocket event: %d %s\n", static_cast<int>(event),
                    data.c_str());
    }
  });

  if (on_connect_) {
    on_connect_();
  }
}
