#include "acoustics/common/DeviceRegistry.h"
#include "acoustics/osc/OscPacket.h"
#include "acoustics/osc/OscTransport.h"

#include "CLI11.hpp"

#include <asio.hpp>
#include <spdlog/spdlog.h>

#include "json.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <condition_variable>
#include <cmath>
#include <csignal>
#include <cstdint>
#include <ctime>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <vector>

using json = nlohmann::json;

namespace {

std::atomic_bool g_shouldStop{false};

void handleSignal(int) {
    g_shouldStop.store(true);
}

struct MonitorOptions {
    std::string listenHost{"0.0.0.0"};
    std::uint16_t port{19100};
    std::optional<std::filesystem::path> csv;
    std::uint64_t maxPackets{0};
    bool quiet{false};
    bool debug{false};
    std::filesystem::path registryPath{"state/devices.json"};
    bool wsEnabled{false};
    std::string wsHost{"127.0.0.1"};
    std::uint16_t wsPort{48080};
    std::string wsPath{"/ws/events"};
};

struct DeviceStats {
    std::uint64_t count{0};
    double meanMs{0.0};
    double m2{0.0};
};

std::string formatIso8601(std::chrono::system_clock::time_point tp) {
    auto timeT = std::chrono::system_clock::to_time_t(tp);
    std::tm utc{};
#if defined(_WIN32)
    gmtime_s(&utc, &timeT);
#else
    gmtime_r(&timeT, &utc);
#endif
    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", &utc);
    auto micros = std::chrono::duration_cast<std::chrono::microseconds>(tp.time_since_epoch()) % 1'000'000;
    std::ostringstream oss;
    oss << buffer << "." << std::setw(6) << std::setfill('0') << micros.count() << "Z";
    return oss.str();
}

class Sha1 {
public:
    void update(const std::uint8_t* data, std::size_t len) {
        bitLength_ += static_cast<std::uint64_t>(len) * 8ULL;
        while (len > 0) {
            const std::size_t toCopy = std::min<std::size_t>(64 - bufferSize_, len);
            std::memcpy(buffer_.data() + bufferSize_, data, toCopy);
            bufferSize_ += toCopy;
            data += toCopy;
            len -= toCopy;
            if (bufferSize_ == 64) {
                processBlock(buffer_.data());
                bufferSize_ = 0;
            }
        }
    }

    std::array<std::uint8_t, 20> finalize() {
        buffer_[bufferSize_++] = 0x80;
        if (bufferSize_ > 56) {
            while (bufferSize_ < 64) {
                buffer_[bufferSize_++] = 0x00;
            }
            processBlock(buffer_.data());
            bufferSize_ = 0;
        }
        while (bufferSize_ < 56) {
            buffer_[bufferSize_++] = 0x00;
        }
        for (int i = 7; i >= 0; --i) {
            buffer_[bufferSize_++] = static_cast<std::uint8_t>((bitLength_ >> (static_cast<std::uint64_t>(i) * 8ULL)) & 0xFFULL);
        }
        processBlock(buffer_.data());

        std::array<std::uint8_t, 20> digest{};
        for (int i = 0; i < 5; ++i) {
            digest[i * 4 + 0] = static_cast<std::uint8_t>((state_[i] >> 24) & 0xFF);
            digest[i * 4 + 1] = static_cast<std::uint8_t>((state_[i] >> 16) & 0xFF);
            digest[i * 4 + 2] = static_cast<std::uint8_t>((state_[i] >> 8) & 0xFF);
            digest[i * 4 + 3] = static_cast<std::uint8_t>((state_[i]) & 0xFF);
        }
        return digest;
    }

private:
    static std::uint32_t leftRotate(std::uint32_t value, std::uint32_t bits) {
        return (value << bits) | (value >> (32 - bits));
    }

    void processBlock(const std::uint8_t* block) {
        std::uint32_t w[80];
        for (int i = 0; i < 16; ++i) {
            w[i] = (static_cast<std::uint32_t>(block[i * 4 + 0]) << 24) |
                   (static_cast<std::uint32_t>(block[i * 4 + 1]) << 16) |
                   (static_cast<std::uint32_t>(block[i * 4 + 2]) << 8) |
                   (static_cast<std::uint32_t>(block[i * 4 + 3]));
        }
        for (int i = 16; i < 80; ++i) {
            w[i] = leftRotate(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        }

        std::uint32_t a = state_[0];
        std::uint32_t b = state_[1];
        std::uint32_t c = state_[2];
        std::uint32_t d = state_[3];
        std::uint32_t e = state_[4];

        for (int i = 0; i < 80; ++i) {
            std::uint32_t f = 0;
            std::uint32_t k = 0;
            if (i < 20) {
                f = (b & c) | ((~b) & d);
                k = 0x5A827999;
            } else if (i < 40) {
                f = b ^ c ^ d;
                k = 0x6ED9EBA1;
            } else if (i < 60) {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8F1BBCDC;
            } else {
                f = b ^ c ^ d;
                k = 0xCA62C1D6;
            }
            std::uint32_t temp = leftRotate(a, 5) + f + e + k + w[i];
            e = d;
            d = c;
            c = leftRotate(b, 30);
            b = a;
            a = temp;
        }

        state_[0] += a;
        state_[1] += b;
        state_[2] += c;
        state_[3] += d;
        state_[4] += e;
    }

    std::array<std::uint8_t, 64> buffer_{};
    std::size_t bufferSize_{0};
    std::uint64_t bitLength_{0};
    std::array<std::uint32_t, 5> state_{{
        0x67452301u,
        0xEFCDAB89u,
        0x98BADCFEu,
        0x10325476u,
        0xC3D2E1F0u
    }};
};

std::array<std::uint8_t, 20> sha1Digest(const std::uint8_t* data, std::size_t len) {
    Sha1 sha;
    sha.update(data, len);
    return sha.finalize();
}

std::string base64Encode(const std::uint8_t* data, std::size_t len) {
    static constexpr char kAlphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";
    std::string encoded;
    encoded.reserve(((len + 2) / 3) * 4);
    std::size_t index = 0;
    while (index < len) {
        const std::size_t chunk = std::min<std::size_t>(3, len - index);
        std::uint32_t block = 0;
        for (std::size_t i = 0; i < chunk; ++i) {
            block |= static_cast<std::uint32_t>(data[index + i]) << (16 - static_cast<std::uint32_t>(i) * 8);
        }
        encoded.push_back(kAlphabet[(block >> 18) & 0x3F]);
        encoded.push_back(kAlphabet[(block >> 12) & 0x3F]);
        if (chunk >= 2) {
            encoded.push_back(kAlphabet[(block >> 6) & 0x3F]);
        } else {
            encoded.push_back('=');
        }
        if (chunk == 3) {
            encoded.push_back(kAlphabet[block & 0x3F]);
        } else {
            encoded.push_back('=');
        }
        index += chunk;
    }
    return encoded;
}

std::string computeWebSocketAcceptKey(const std::string& clientKey) {
    static constexpr char kGuid[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    std::string input = clientKey + kGuid;
    auto digest = sha1Digest(reinterpret_cast<const std::uint8_t*>(input.data()), input.size());
    return base64Encode(digest.data(), digest.size());
}

class WebSocketBroadcaster {
public:
    WebSocketBroadcaster(const std::string& host, std::uint16_t port, std::string path)
        : endpoint_(asio::ip::make_address(host), port), path_(std::move(path)) {}

    ~WebSocketBroadcaster() {
        stop();
    }

    void start() {
        if (running_.load()) {
            return;
        }
        acceptor_ = std::make_unique<asio::ip::tcp::acceptor>(ioContext_);
        acceptor_->open(endpoint_.protocol());
        acceptor_->set_option(asio::socket_base::reuse_address(true));
        acceptor_->bind(endpoint_);
        acceptor_->listen();

        running_.store(true);
        acceptThread_ = std::thread([this]() { acceptLoop(); });
        dispatchThread_ = std::thread([this]() { dispatchLoop(); });
        spdlog::info("WebSocket broadcaster listening on {}:{}", endpoint_.address().to_string(), endpoint_.port());
    }

    void stop() {
        if (!running_.exchange(false)) {
            return;
        }
        {
            std::lock_guard queueLock(queueMutex_);
            queueCv_.notify_all();
        }
        if (acceptor_) {
            std::error_code ec;
            acceptor_->close(ec);
        }
        if (acceptThread_.joinable()) {
            acceptThread_.join();
        }
        if (dispatchThread_.joinable()) {
            dispatchThread_.join();
        }
        std::lock_guard clientLock(clientsMutex_);
        for (auto& client : clients_) {
            if (client && client->socket && client->socket->is_open()) {
                std::error_code ec;
                client->socket->close(ec);
            }
        }
        clients_.clear();
        acceptor_.reset();
    }

    void broadcast(const json& payload) {
        if (!running_.load()) {
            return;
        }
        enqueueMessage(payload.dump());
    }

    void setDeviceCountProvider(std::function<std::size_t()> provider) {
        deviceCountProvider_ = std::move(provider);
    }

private:
    struct Client {
        explicit Client(std::shared_ptr<asio::ip::tcp::socket> sock)
            : socket(std::move(sock)) {}
        std::shared_ptr<asio::ip::tcp::socket> socket;
        std::mutex writeMutex;
    };

    void enqueueMessage(std::string message) {
        {
            std::lock_guard lock(queueMutex_);
            queue_.push_back(std::move(message));
        }
        queueCv_.notify_one();
    }

    void acceptLoop() {
        while (running_.load()) {
            asio::ip::tcp::socket socket(ioContext_);
            std::error_code ec;
            acceptor_->accept(socket, ec);
            if (ec) {
                if (running_.load()) {
                    spdlog::warn("WebSocket accept error: {}", ec.message());
                }
                continue;
            }
            std::thread(&WebSocketBroadcaster::serveClient, this, std::move(socket)).detach();
        }
    }

    void serveClient(asio::ip::tcp::socket socket) {
        try {
            auto request = readHttpRequest(socket);
            const auto requestLineEnd = request.find("\r\n");
            if (requestLineEnd == std::string::npos) {
                throw std::runtime_error("Malformed HTTP request");
            }
            const std::string requestLine = request.substr(0, requestLineEnd);
            const std::string path = parseRequestPath(requestLine);
            if (path != path_) {
                sendHttpError(socket, "404 Not Found", "Unsupported path");
                return;
            }
            const auto clientKey = getHeaderValue(request, "sec-websocket-key");
            if (clientKey.empty()) {
                sendHttpError(socket, "400 Bad Request", "Missing Sec-WebSocket-Key");
                return;
            }
            const auto acceptKey = computeWebSocketAcceptKey(clientKey);
            sendHandshake(socket, acceptKey);
            auto client = std::make_shared<Client>(std::make_shared<asio::ip::tcp::socket>(std::move(socket)));
            {
                std::lock_guard lock(clientsMutex_);
                clients_.push_back(client);
            }
            sendHello(client);
        } catch (const std::exception& ex) {
            spdlog::warn("WebSocket client setup failed: {}", ex.what());
        }
    }

    void dispatchLoop() {
        std::unique_lock lock(queueMutex_);
        while (running_.load()) {
            queueCv_.wait(lock, [&]() { return !running_.load() || !queue_.empty(); });
            if (!running_.load()) {
                break;
            }
            std::string message = std::move(queue_.front());
            queue_.pop_front();
            lock.unlock();
            sendToClients(message);
            lock.lock();
        }
    }

    void sendToClients(const std::string& message) {
        auto frame = buildFrame(message);
        std::lock_guard lock(clientsMutex_);
        for (auto it = clients_.begin(); it != clients_.end();) {
            auto client = *it;
            if (!client || !client->socket || !client->socket->is_open()) {
                it = clients_.erase(it);
                continue;
            }
            std::error_code ec;
            {
                std::lock_guard writeLock(client->writeMutex);
                asio::write(*client->socket, asio::buffer(frame), ec);
            }
            if (ec) {
                spdlog::debug("WebSocket send failed: {}", ec.message());
                std::error_code closeEc;
                client->socket->close(closeEc);
                it = clients_.erase(it);
            } else {
                ++it;
            }
        }
    }

    static std::vector<std::uint8_t> buildFrame(const std::string& payload) {
        std::vector<std::uint8_t> frame;
        const std::size_t len = payload.size();
        frame.reserve(2 + len + 8);
        frame.push_back(0x81);  // FIN + text frame
        if (len <= 125) {
            frame.push_back(static_cast<std::uint8_t>(len));
        } else if (len <= 0xFFFF) {
            frame.push_back(126);
            frame.push_back(static_cast<std::uint8_t>((len >> 8) & 0xFF));
            frame.push_back(static_cast<std::uint8_t>(len & 0xFF));
        } else {
            frame.push_back(127);
            for (int i = 7; i >= 0; --i) {
                frame.push_back(static_cast<std::uint8_t>((len >> (i * 8)) & 0xFF));
            }
        }
        frame.insert(frame.end(), payload.begin(), payload.end());
        return frame;
    }

    static std::string readHttpRequest(asio::ip::tcp::socket& socket) {
        std::string request;
        std::array<char, 1024> buffer{};
        while (request.find("\r\n\r\n") == std::string::npos) {
            std::error_code ec;
            std::size_t bytes = socket.read_some(asio::buffer(buffer), ec);
            if (ec) {
                throw std::runtime_error("HTTP read failed: " + ec.message());
            }
            request.append(buffer.data(), bytes);
            if (request.size() > 16 * 1024) {
                throw std::runtime_error("HTTP header too large");
            }
        }
        return request;
    }

    static std::string getHeaderValue(const std::string& request, const std::string& key) {
        auto lowerKey = key;
        std::transform(lowerKey.begin(), lowerKey.end(), lowerKey.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        std::istringstream stream(request);
        std::string line;
        while (std::getline(stream, line)) {
            if (line.size() >= 2 && line.back() == '\r') {
                line.pop_back();
            }
            auto colon = line.find(':');
            if (colon == std::string::npos) {
                continue;
            }
            std::string headerKey = line.substr(0, colon);
            std::string value = line.substr(colon + 1);
            auto normalize = [](std::string s) {
                s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) { return !std::isspace(ch); }));
                s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), s.end());
                std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                return s;
            };
            if (normalize(headerKey) == lowerKey) {
                value.erase(value.begin(), std::find_if(value.begin(), value.end(), [](unsigned char ch) { return !std::isspace(ch); }));
                value.erase(std::find_if(value.rbegin(), value.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), value.end());
                return value;
            }
        }
        return {};
    }

    static std::string parseRequestPath(const std::string& requestLine) {
        constexpr char kGet[] = "GET ";
        if (requestLine.rfind(kGet, 0) != 0) {
            throw std::runtime_error("Unsupported HTTP verb");
        }
        auto pathStart = sizeof(kGet) - 1;
        auto spacePos = requestLine.find(' ', pathStart);
        if (spacePos == std::string::npos) {
            throw std::runtime_error("Malformed request line");
        }
        return requestLine.substr(pathStart, spacePos - pathStart);
    }

    static void sendHttpError(asio::ip::tcp::socket& socket, const std::string& status, const std::string& body) {
        std::ostringstream oss;
        oss << "HTTP/1.1 " << status << "\r\n"
            << "Content-Type: text/plain\r\n"
            << "Content-Length: " << body.size() << "\r\n"
            << "Connection: close\r\n\r\n"
            << body;
        std::error_code ec;
        asio::write(socket, asio::buffer(oss.str()), ec);
        socket.close(ec);
    }

    static void sendHandshake(asio::ip::tcp::socket& socket, const std::string& acceptKey) {
        std::ostringstream oss;
        oss << "HTTP/1.1 101 Switching Protocols\r\n"
            << "Upgrade: websocket\r\n"
            << "Connection: Upgrade\r\n"
            << "Sec-WebSocket-Accept: " << acceptKey << "\r\n"
            << "\r\n";
        asio::write(socket, asio::buffer(oss.str()));
    }

    void sendHello(const std::shared_ptr<Client>& client) {
        if (!client || !client->socket || !client->socket->is_open()) {
            return;
        }
        std::size_t deviceCount = 0;
        if (deviceCountProvider_) {
            deviceCount = deviceCountProvider_();
        }
        json hello{
            {"type", "hello"},
            {"device_count", deviceCount}
        };
        auto frame = buildFrame(hello.dump());
        std::error_code ec;
        {
            std::lock_guard writeLock(client->writeMutex);
            asio::write(*client->socket, asio::buffer(frame), ec);
        }
        if (ec) {
            std::lock_guard lock(clientsMutex_);
            auto it = std::find(clients_.begin(), clients_.end(), client);
            if (it != clients_.end()) {
                clients_.erase(it);
            }
        }
    }

    asio::ip::tcp::endpoint endpoint_;
    std::string path_;
    std::atomic_bool running_{false};
    asio::io_context ioContext_;
    std::unique_ptr<asio::ip::tcp::acceptor> acceptor_;
    std::thread acceptThread_;
    std::thread dispatchThread_;
    std::mutex clientsMutex_;
    std::vector<std::shared_ptr<Client>> clients_;
    std::mutex queueMutex_;
    std::condition_variable queueCv_;
    std::deque<std::string> queue_;
    std::function<std::size_t()> deviceCountProvider_;
};

std::chrono::system_clock::time_point secondsToTimePoint(double seconds) {
    auto secs = static_cast<std::int64_t>(seconds);
    double fractional = seconds - static_cast<double>(secs);
    auto tp = std::chrono::system_clock::time_point{std::chrono::seconds{secs}} +
              std::chrono::duration_cast<std::chrono::system_clock::duration>(
                  std::chrono::duration<double>(fractional));
    return tp;
}

double toEpochSeconds(std::chrono::system_clock::time_point tp) {
    auto duration = tp.time_since_epoch();
    return std::chrono::duration<double>(duration).count();
}

double argumentToSeconds(const acoustics::osc::Argument& arg) {
    if (const auto* f = std::get_if<float>(&arg)) {
        return static_cast<double>(*f);
    }
    if (const auto* i = std::get_if<std::int32_t>(&arg)) {
        return static_cast<double>(*i);
    }
    throw std::runtime_error("Unsupported timestamp argument type");
}

void updateStats(DeviceStats& stats, double sampleMs) {
    ++stats.count;
    double delta = sampleMs - stats.meanMs;
    stats.meanMs += delta / static_cast<double>(stats.count);
    double delta2 = sampleMs - stats.meanMs;
    stats.m2 += delta * delta2;
}

std::ofstream openCsv(const std::filesystem::path& path) {
    const bool exists = std::filesystem::exists(path);
    std::ofstream out(path, std::ios::app);
    if (!out) {
        throw std::runtime_error("Failed to open CSV file: " + path.string());
    }
    if (!exists) {
        out << "arrival_iso,device_id,sequence,latency_ms,sent_iso\n";
    }
    return out;
}

struct HeartbeatData {
    std::string deviceId;
    std::int32_t sequence;
    double sentSeconds;
    std::optional<std::int32_t> queueSize;
    std::optional<bool> isPlaying;
};

std::string describeArgument(const acoustics::osc::Argument& arg) {
    return std::visit(
        [](const auto& value) -> std::string {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, std::int32_t>) {
                return "int32(" + std::to_string(value) + ")";
            } else if constexpr (std::is_same_v<T, float>) {
                return "float(" + std::to_string(value) + ")";
            } else if constexpr (std::is_same_v<T, std::string>) {
                return "string(\"" + value + "\")";
            } else if constexpr (std::is_same_v<T, bool>) {
                return std::string("bool(") + (value ? "true" : "false") + ")";
            } else if constexpr (std::is_same_v<T, acoustics::osc::Blob>) {
                return "blob(size=" + std::to_string(value.size()) + ")";
            }
            return "unknown";
        },
        arg);
}

HeartbeatData parseHeartbeat(const acoustics::osc::Message& message) {
    if (message.address != "/heartbeat" || message.arguments.size() < 3) {
        throw std::runtime_error("Not a heartbeat message");
    }
    HeartbeatData data;
    if (const auto* id = std::get_if<std::string>(&message.arguments[0])) {
        data.deviceId = *id;
    } else {
        throw std::runtime_error("Heartbeat device id must be a string");
    }
    if (const auto* seq = std::get_if<std::int32_t>(&message.arguments[1])) {
        data.sequence = *seq;
    } else {
        throw std::runtime_error("Heartbeat sequence must be int32");
    }

    if (message.arguments.size() >= 4 &&
        std::holds_alternative<std::int32_t>(message.arguments[2]) &&
        std::holds_alternative<std::int32_t>(message.arguments[3])) {
        auto secs = std::get<std::int32_t>(message.arguments[2]);
        auto micros = std::get<std::int32_t>(message.arguments[3]);
        data.sentSeconds = static_cast<double>(secs) +
                          static_cast<double>(micros) / 1'000'000.0;
    } else {
        data.sentSeconds = argumentToSeconds(message.arguments[2]);
    }

    if (message.arguments.size() >= 5) {
        if (const auto* queue = std::get_if<std::int32_t>(&message.arguments[4])) {
            data.queueSize = *queue;
        }
    }

    if (message.arguments.size() >= 6) {
        if (const auto* playingBool = std::get_if<bool>(&message.arguments[5])) {
            data.isPlaying = *playingBool;
        } else if (const auto* playingInt = std::get_if<std::int32_t>(&message.arguments[5])) {
            data.isPlaying = (*playingInt != 0);
        } else if (const auto* playingFloat = std::get_if<float>(&message.arguments[5])) {
            data.isPlaying = (*playingFloat != 0.0f);
        }
    }
    return data;
}

void emitSample(std::ostream& out,
                const HeartbeatData& data,
                double latencyMs,
                std::chrono::system_clock::time_point arrival) {
    auto arrivalTimeT = std::chrono::system_clock::to_time_t(arrival);
    auto arrivalLocal = *std::localtime(&arrivalTimeT);
    auto sentTimeT = std::chrono::system_clock::to_time_t(secondsToTimePoint(data.sentSeconds));
    auto sentLocal = *std::localtime(&sentTimeT);

    out << std::put_time(&arrivalLocal, "%Y-%m-%d %H:%M:%S")
        << "," << data.deviceId
        << "," << data.sequence
        << "," << std::fixed << std::setprecision(3) << latencyMs
        << "," << std::put_time(&sentLocal, "%Y-%m-%d %H:%M:%S")
        << '\n';
}

std::optional<json> processMessage(const acoustics::osc::Message& message,
                    const MonitorOptions& options,
                    std::unordered_map<std::string, DeviceStats>& stats,
                    std::ofstream* csvStream,
                    acoustics::common::DeviceRegistry* registry) {
    spdlog::debug("processMessage: address={} arg_count={}", message.address, message.arguments.size());

    HeartbeatData data;
    try {
        data = parseHeartbeat(message);
    } catch (const std::exception& ex) {
        std::ostringstream argStream;
        for (std::size_t i = 0; i < message.arguments.size(); ++i) {
            if (i > 0) {
                argStream << ", ";
            }
            argStream << describeArgument(message.arguments[i]);
        }
        spdlog::warn("Failed to parse heartbeat: {} (address={} args=[{}])",
                     ex.what(),
                     message.address,
                     argStream.str());
        return std::nullopt;
    }

    auto arrival = std::chrono::system_clock::now();
    double arrivalSeconds = toEpochSeconds(arrival);
    double latencyMs = (arrivalSeconds - data.sentSeconds) * 1000.0;

    auto& deviceStats = stats[data.deviceId];
    updateStats(deviceStats, latencyMs);
    if (data.queueSize || data.isPlaying.has_value()) {
        spdlog::debug("Heartbeat parsed: id={} seq={} sent_seconds={:.6f} latency_ms={:.3f} count={} queue={} playing={}",
                      data.deviceId,
                      data.sequence,
                      data.sentSeconds,
                      latencyMs,
                      deviceStats.count,
                      data.queueSize ? std::to_string(*data.queueSize) : "n/a",
                      data.isPlaying.has_value() ? (data.isPlaying.value() ? "yes" : "no") : "n/a");
    } else {
        spdlog::debug("Heartbeat parsed: id={} seq={} sent_seconds={:.6f} latency_ms={:.3f} count={}",
                      data.deviceId,
                      data.sequence,
                      data.sentSeconds,
                      latencyMs,
                      deviceStats.count);
    }

    if (registry) {
        registry->recordHeartbeat(data.deviceId, latencyMs, arrival);
    }

    if (!options.quiet) {
        std::cout << "[" << data.deviceId << "] seq=" << data.sequence
                  << " latency=" << std::fixed << std::setprecision(3) << latencyMs << " ms";
        if (data.queueSize) {
            std::cout << " queue=" << *data.queueSize;
        }
        if (data.isPlaying.has_value()) {
            std::cout << " playing=" << (data.isPlaying.value() ? "yes" : "no");
        }
        std::cout << std::endl;
    }

    if (csvStream) {
        emitSample(*csvStream, data, latencyMs, arrival);
        csvStream->flush();
    }

    json payload{
        {"type", "heartbeat"},
        {"device_id", data.deviceId},
        {"sequence", data.sequence},
        {"latency_ms", latencyMs},
        {"timestamp", formatIso8601(arrival)},
        {"sent_timestamp", formatIso8601(secondsToTimePoint(data.sentSeconds))}
    };
    if (data.queueSize) {
        payload["queue_depth"] = *data.queueSize;
    }
    if (data.isPlaying.has_value()) {
        payload["is_playing"] = *data.isPlaying;
    }
    return payload;
}

void processAnnounce(const acoustics::osc::Message& message,
                     const MonitorOptions& options,
                     acoustics::common::DeviceRegistry& registry) {
    if (message.arguments.empty()) {
        if (!options.quiet) {
            std::cerr << "Announce message missing arguments" << std::endl;
        }
        return;
    }

    auto getStringArg = [&](std::size_t index) -> std::optional<std::string> {
        if (index >= message.arguments.size()) {
            return std::nullopt;
        }
        if (const auto* value = std::get_if<std::string>(&message.arguments[index])) {
            return *value;
        }
        return std::nullopt;
    };

    auto looksLikeMac = [](const std::string& text) {
        return text.find(':') != std::string::npos;
    };

    std::optional<std::string> deviceId = getStringArg(0);
    if (!deviceId) {
        if (!options.quiet) {
            std::cerr << "Announce first argument must be string" << std::endl;
        }
        return;
    }

    std::optional<std::string> macArg;
    std::size_t nextIndex = 1;

    if (looksLikeMac(*deviceId)) {
        macArg = deviceId;
        deviceId = std::nullopt;
        auto maybeSecond = getStringArg(1);
        if (maybeSecond && !looksLikeMac(*maybeSecond)) {
            deviceId = maybeSecond;
            nextIndex = 2;
        }
    } else {
        macArg = getStringArg(1);
        nextIndex = 2;
    }

    if (!macArg) {
        if (!options.quiet) {
            std::cerr << "Announce message missing MAC address" << std::endl;
        }
        return;
    }

    std::string fwVersion;
    if (auto maybeFw = getStringArg(nextIndex)) {
        fwVersion = *maybeFw;
        ++nextIndex;
    }

    std::optional<std::string> alias;
    if (auto maybeAlias = getStringArg(nextIndex)) {
        alias = *maybeAlias;
    }
    if (!alias && deviceId) {
        alias = *deviceId;
    }

    auto now = std::chrono::system_clock::now();
    auto state = registry.registerAnnounce(*macArg, fwVersion, alias, now);
    if (!options.quiet) {
        std::cout << "ANNOUNCE id=" << (deviceId ? *deviceId : state.id)
                  << " mac=" << state.mac
                  << " fw=" << state.firmwareVersion;
        if (state.alias) {
            std::cout << " alias=" << *state.alias;
        }
        std::cout << std::endl;
    }
}

std::vector<json> processPacket(const acoustics::osc::Packet& packet,
                   const MonitorOptions& options,
                   std::unordered_map<std::string, DeviceStats>& stats,
                   std::ofstream* csvStream,
                   acoustics::common::DeviceRegistry* registry) {
    std::vector<json> events;
    auto handle = [&](const acoustics::osc::Message& msg) {
        if (spdlog::should_log(spdlog::level::debug)) {
            std::ostringstream argStream;
            for (std::size_t i = 0; i < msg.arguments.size(); ++i) {
                if (i > 0) {
                    argStream << ", ";
                }
                argStream << describeArgument(msg.arguments[i]);
            }
            spdlog::debug("processPacket: dispatching address={} args=[{}]", msg.address, argStream.str());
        }
        if (registry && msg.address == "/announce") {
            processAnnounce(msg, options, *registry);
            return;
        }
        if (auto payload = processMessage(msg, options, stats, csvStream, registry)) {
            events.push_back(std::move(*payload));
        }
    };

    if (const auto* message = std::get_if<acoustics::osc::Message>(&packet)) {
        handle(*message);
    } else if (const auto* bundle = std::get_if<acoustics::osc::Bundle>(&packet)) {
        spdlog::debug("processPacket: bundle with {} elements", bundle->elements.size());
        for (const auto& msg : bundle->elements) {
            handle(msg);
        }
    }
    return events;
}

void printSummary(const std::unordered_map<std::string, DeviceStats>& stats) {
    if (stats.empty()) {
        std::cout << "No heartbeat samples captured." << std::endl;
        return;
    }

    std::cout << "\nLatency summary (ms):\n";
    std::cout << std::left << std::setw(20) << "Device"
              << std::right << std::setw(10) << "Count"
              << std::setw(15) << "Mean"
              << std::setw(15) << "StdDev" << '\n';

    for (const auto& [device, stat] : stats) {
        double stddev = 0.0;
        if (stat.count > 1) {
            stddev = std::sqrt(stat.m2 / static_cast<double>(stat.count - 1));
        }
        std::cout << std::left << std::setw(20) << device
                  << std::right << std::setw(10) << stat.count
                  << std::setw(15) << std::fixed << std::setprecision(3) << stat.meanMs
                  << std::setw(15) << std::fixed << std::setprecision(3) << stddev
                  << '\n';
    }
}

}  // namespace

int main(int argc, char** argv) {
    CLI::App app{"Agent A Heartbeat Monitor"};
    MonitorOptions options;

    app.add_option("--host", options.listenHost, "Listen address (IPv4)");
    app.add_option("--port", options.port, "Listen port");
    app.add_option("--csv", options.csv, "Append results to CSV file");
    app.add_option("--count", options.maxPackets, "Stop after N packets (0 = unlimited)");
    app.add_flag("--quiet", options.quiet, "Suppress console output");
    app.add_flag("--debug", options.debug, "Enable verbose debug logging");
    app.add_option("--registry", options.registryPath, "Device registry JSON path");
    app.add_flag("--ws", options.wsEnabled, "Enable WebSocket event broadcasting");
    app.add_option("--ws-host", options.wsHost, "WebSocket listen host (default: 127.0.0.1)");
    app.add_option("--ws-port", options.wsPort, "WebSocket listen port (default: 48080)");
    app.add_option("--ws-path", options.wsPath, "WebSocket path (default: /ws/events)");

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& e) {
        return app.exit(e);
    }

    spdlog::set_pattern("%Y-%m-%d %H:%M:%S.%e [%^%l%$] %v");
    if (options.debug) {
        spdlog::set_level(spdlog::level::debug);
        spdlog::debug("Debug logging enabled");
    } else {
        spdlog::set_level(spdlog::level::info);
    }

    std::signal(SIGINT, handleSignal);

    try {
        std::unique_ptr<std::ofstream> csvStream;
        if (options.csv.has_value()) {
            csvStream = std::make_unique<std::ofstream>(openCsv(*options.csv));
        }

        acoustics::common::DeviceRegistry registry(options.registryPath);
        registry.load();
        std::unique_ptr<WebSocketBroadcaster> wsBroadcaster;
        if (options.wsEnabled) {
            try {
                wsBroadcaster = std::make_unique<WebSocketBroadcaster>(options.wsHost, options.wsPort, options.wsPath);
                wsBroadcaster->setDeviceCountProvider([&registry]() {
                    return registry.snapshot().size();
                });
                wsBroadcaster->start();
            } catch (const std::exception& ex) {
                throw std::runtime_error(std::string("Failed to start WebSocket broadcaster: ") + ex.what());
            }
        }

        std::unordered_map<std::string, DeviceStats> stats;
        std::mutex stateMutex;
        std::atomic_uint64_t processed{0};

        asio::ip::address listenAddress;
        try {
            listenAddress = asio::ip::make_address(options.listenHost);
        } catch (const std::exception& ex) {
            throw std::runtime_error("Invalid listen address: " + options.listenHost + " (" + ex.what() + ")");
        }

        acoustics::osc::IoContextRunner runner;
        WebSocketBroadcaster* wsRaw = wsBroadcaster.get();

        acoustics::osc::OscListener listener(
            runner.context(),
            acoustics::osc::OscListener::Endpoint(listenAddress, options.port),
            [&](const acoustics::osc::Packet& packet, const acoustics::osc::OscListener::Endpoint&) {
                std::vector<json> events;
                {
                    std::lock_guard lock(stateMutex);
                    events = processPacket(packet,
                                           options,
                                           stats,
                                           csvStream ? csvStream.get() : nullptr,
                                           &registry);
                    ++processed;
                    if (options.maxPackets > 0 && processed.load() >= options.maxPackets) {
                        g_shouldStop.store(true);
                    }
                }
                if (wsRaw) {
                    for (auto& evt : events) {
                        wsRaw->broadcast(evt);
                    }
                }
            });

        listener.start();
        runner.start();

        while (!g_shouldStop.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            if (options.maxPackets > 0 && processed.load() >= options.maxPackets) {
                break;
            }
        }

        listener.stop();
        runner.stop();
        if (wsBroadcaster) {
            wsBroadcaster->stop();
        }

        if (!options.quiet) {
            std::lock_guard lock(stateMutex);
            printSummary(stats);
        }
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << '\n';
        return 1;
    }

    return 0;
}
