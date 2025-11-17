#include "toio_control/scheduler/osc/osc_bundle_sender.h"

#include "toio_control/osc/OscTransport.h"

#include <asio.hpp>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <thread>

namespace toio_control::scheduler::osc {

OscBundleSender::OscBundleSender(const std::string& host, std::uint16_t port, bool broadcast)
    : host_(host), port_(port), broadcast_(broadcast) {
    
    try {
        ioContextRunner_ = std::make_unique<toio_control::osc::IoContextRunner>();
        ioContextRunner_->start();
        
        asio::ip::address address = asio::ip::make_address(host);
        toio_control::osc::OscSender::Endpoint endpoint(address, port);
        sender_ = std::make_unique<toio_control::osc::OscSender>(
            ioContextRunner_->context(), endpoint, broadcast);
        
        connected_ = true;
        spdlog::info("OSC bundle sender initialized: {}:{} (broadcast: {})", host, port, broadcast);
    } catch (const std::exception& ex) {
        spdlog::error("Failed to initialize OSC bundle sender: {}", ex.what());
        connected_ = false;
        throw std::runtime_error("Failed to initialize OSC bundle sender: " + std::string(ex.what()));
    }
}

OscBundleSender::~OscBundleSender() {
    if (ioContextRunner_) {
        ioContextRunner_->stop();
    }
}

void OscBundleSender::enableEncryption(const toio_control::osc::OscEncryptor::Key256& key,
                                       const toio_control::osc::OscEncryptor::Iv128& iv) {
    if (sender_) {
        sender_->enableEncryption(key, iv);
        encryptionEnabled_ = true;
        spdlog::info("OSC bundle sender encryption enabled");
    }
}

void OscBundleSender::disableEncryption() {
    if (sender_) {
        sender_->disableEncryption();
        encryptionEnabled_ = false;
        spdlog::info("OSC bundle sender encryption disabled");
    }
}

bool OscBundleSender::encryptionEnabled() const {
    if (sender_) {
        return sender_->encryptionEnabled();
    }
    return encryptionEnabled_;
}

void OscBundleSender::sendBundle(const toio_control::osc::Bundle& bundle) {
    if (!connected_ || !sender_) {
        throw std::runtime_error("OSC bundle sender not connected");
    }
    
    try {
        sender_->send(bundle);
    } catch (const std::exception& ex) {
        spdlog::warn("OSC bundle send failed: {}", ex.what());
        throw;
    }
}

void OscBundleSender::sendBundles(const std::vector<toio_control::osc::Bundle>& bundles,
                                   double bundleSpacing) {
    if (bundleSpacing < 0.01) {
        throw std::runtime_error("Bundle spacing must be at least 0.01 seconds");
    }
    
    for (std::size_t i = 0; i < bundles.size(); ++i) {
        sendBundle(bundles[i]);
        
        if (bundleSpacing > 0.0 && i + 1 < bundles.size()) {
            std::this_thread::sleep_for(std::chrono::duration<double>(bundleSpacing));
        }
    }
    
    spdlog::debug("Sent {} OSC bundles", bundles.size());
}

bool OscBundleSender::isConnected() const {
    return connected_;
}

void OscBundleSender::disconnect() {
    if (ioContextRunner_) {
        ioContextRunner_->stop();
        ioContextRunner_.reset();
    }
    sender_.reset();
    connected_ = false;
    spdlog::debug("OSC bundle sender disconnected");
}

}  // namespace toio_control::scheduler::osc

