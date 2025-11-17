#include "osc_sender.hpp"
#include <spdlog/spdlog.h>
#include <chrono>

namespace swarm_control {

OscSender::OscSender(const std::string& address, int port)
    : address_(address), port_(port) {
    
    try {
        ioContextRunner_ = std::make_unique<toio_control::osc::IoContextRunner>();
        ioContextRunner_->start();
        
        asio::ip::address addr = asio::ip::make_address(address);
        toio_control::osc::OscSender::Endpoint endpoint(addr, static_cast<unsigned short>(port));
        sender_ = std::make_unique<toio_control::osc::OscSender>(
            ioContextRunner_->context(), endpoint, false);
        
        connected_ = true;
        spdlog::info("OSC sender initialized: {}:{}", address, port);
    } catch (const std::exception& ex) {
        spdlog::error("Failed to initialize OSC sender: {}", ex.what());
        connected_ = false;
    }
}

void OscSender::sendMessage(const toio_control::osc::Message& message) {
    if (!connected_ || !sender_) {
        return;
    }
    
    try {
        sender_->send(message);
    } catch (const std::exception& ex) {
        spdlog::warn("OSC send failed: {}", ex.what());
    }
}

void OscSender::sendBundle(const toio_control::osc::Bundle& bundle) {
    if (!connected_ || !sender_) {
        return;
    }
    
    try {
        sender_->send(bundle);
    } catch (const std::exception& ex) {
        spdlog::warn("OSC bundle send failed: {}", ex.what());
    }
}

void OscSender::sendTarget(int robotIndex, double x, double y, double angle) {
    toio_control::osc::Message message;
    message.address = getTargetAddress(robotIndex);
    message.arguments = {
        static_cast<float>(x),
        static_cast<float>(y),
        static_cast<float>(angle)
    };
    
    sendMessage(message);
}

void OscSender::sendTargets(const std::vector<TargetPosition>& targets) {
    toio_control::osc::Bundle bundle;
    bundle.timetag = toio_control::osc::toTimetag(std::chrono::system_clock::now());
    
    for (const auto& target : targets) {
        toio_control::osc::Message message;
        message.address = getTargetAddress(target.robotIndex);
        message.arguments = {
            static_cast<float>(target.x),
            static_cast<float>(target.y),
            static_cast<float>(target.angle)
        };
        bundle.elements.push_back(message);
    }
    
    sendBundle(bundle);
}

void OscSender::enableEncryption(const toio_control::osc::OscEncryptor::Key256& key,
                                 const toio_control::osc::OscEncryptor::Iv128& iv) {
    if (sender_) {
        sender_->enableEncryption(key, iv);
        spdlog::info("OSC sender encryption enabled");
    }
}

void OscSender::disableEncryption() {
    if (sender_) {
        sender_->disableEncryption();
        spdlog::info("OSC sender encryption disabled");
    }
}

bool OscSender::encryptionEnabled() const {
    if (sender_) {
        return sender_->encryptionEnabled();
    }
    return false;
}

}  // namespace swarm_control

