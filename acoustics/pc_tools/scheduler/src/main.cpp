#include "acoustics/osc/OscTransport.h"
#include "acoustics/scheduler/SoundTimeline.h"

#include "CLI11.hpp"
#include "json.hpp"

#include <asio.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cctype>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <variant>
#include <vector>

namespace {

using json = nlohmann::json;

using acoustics::scheduler::ScheduledBundle;
using acoustics::scheduler::ScheduledMessage;
using acoustics::scheduler::TargetResolver;

struct SchedulerOptions {
    std::string timelinePath;
    std::string host{"255.255.255.255"};
    std::uint16_t port{9000};
    double leadTime{-1.0};
    double spacing{0.01};
    bool broadcast{true};
    bool dryRun{false};
    std::string baseTimeIso;
    std::string targetMapPath;
    std::vector<std::string> defaultTargets;
};

std::chrono::system_clock::time_point parseIsoTime(std::string value) {
    if (value.empty()) {
        return std::chrono::system_clock::now();
    }

    if (value.back() == 'Z') {
        value.pop_back();
    }

    auto dotPos = value.find('.');
    std::string fractionalPart;
    if (dotPos != std::string::npos) {
        fractionalPart = value.substr(dotPos + 1);
        value = value.substr(0, dotPos);
    }

    std::tm tm{};
    std::istringstream iss(value);
    iss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    if (iss.fail()) {
        throw std::runtime_error("Failed to parse base time. Expected format YYYY-MM-DDTHH:MM:SS[.fff][Z]");
    }

    auto tp = std::chrono::system_clock::from_time_t(std::mktime(&tm));

    if (!fractionalPart.empty()) {
        double fraction = std::stod("0." + fractionalPart);
        tp += std::chrono::duration_cast<std::chrono::system_clock::duration>(
            std::chrono::duration<double>(fraction)
        );
    }

    return tp;
}

std::string trimCopy(const std::string& value) {
    auto first = value.begin();
    while (first != value.end() && std::isspace(static_cast<unsigned char>(*first))) {
        ++first;
    }
    if (first == value.end()) {
        return {};
    }
    auto last = value.end();
    do {
        --last;
    } while (last != first && std::isspace(static_cast<unsigned char>(*last)));
    return std::string(first, last + 1);
}

std::string toLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::unordered_map<std::string, std::vector<std::string>> loadJsonTargetMap(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("Failed to open target JSON mapping: " + path.string());
    }

    json root;
    input >> root;
    if (!root.is_object()) {
        throw std::runtime_error("Target JSON mapping must contain an object at the root");
    }

    std::unordered_map<std::string, std::vector<std::string>> mapping;
    for (const auto& [key, value] : root.items()) {
        std::vector<std::string> deviceIds;
        if (value.is_array()) {
            for (const auto& element : value) {
                if (!element.is_string()) {
                    throw std::runtime_error("Target JSON mapping arrays must contain strings only");
                }
                deviceIds.emplace_back(element.get<std::string>());
            }
        } else if (value.is_string()) {
            deviceIds.emplace_back(value.get<std::string>());
        } else {
            throw std::runtime_error("Target JSON mapping values must be strings or arrays of strings");
        }
        if (!deviceIds.empty()) {
            mapping.emplace(key, std::move(deviceIds));
        }
    }
    return mapping;
}

std::unordered_map<std::string, std::vector<std::string>> loadCsvTargetMap(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("Failed to open target CSV mapping: " + path.string());
    }

    std::unordered_map<std::string, std::vector<std::string>> mapping;
    std::string line;
    std::size_t lineNumber = 0;

    while (std::getline(input, line)) {
        ++lineNumber;
        auto trimmed = trimCopy(line);
        if (trimmed.empty() || trimmed[0] == '#') {
            continue;
        }

        std::stringstream ss(trimmed);
        std::string logical;
        std::string device;

        if (!std::getline(ss, logical, ',')) {
            continue;
        }
        if (!std::getline(ss, device)) {
            throw std::runtime_error("Target CSV mapping line " + std::to_string(lineNumber) + " missing device id");
        }

        logical = trimCopy(logical);
        device = trimCopy(device);

        if (logical.empty() || device.empty()) {
            continue;
        }

        const auto logicalLower = toLower(logical);
        const auto deviceLower = toLower(device);
        if ((logicalLower == "voice" || logicalLower == "logical") &&
            (deviceLower == "device" || deviceLower == "device_id")) {
            // Header row
            continue;
        }

        mapping[logical].push_back(device);
    }

    return mapping;
}

std::unordered_map<std::string, std::vector<std::string>> loadTargetMap(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        throw std::runtime_error("Target mapping file not found: " + path.string());
    }

    auto ext = toLower(path.extension().string());
    if (ext == ".json") {
        return loadJsonTargetMap(path);
    }
    if (ext == ".csv") {
        return loadCsvTargetMap(path);
    }

    try {
        return loadJsonTargetMap(path);
    } catch (...) {
        // fall through and attempt CSV parsing
    }
    return loadCsvTargetMap(path);
}

TargetResolver buildResolver(const SchedulerOptions& opts) {
    TargetResolver resolver;
    if (!opts.targetMapPath.empty()) {
        auto mapping = loadTargetMap(opts.targetMapPath);
        resolver.setMapping(std::move(mapping));
    }
    if (!opts.defaultTargets.empty()) {
        resolver.setDefaultTargets(opts.defaultTargets);
    }
    return resolver;
}

void printArgument(const acoustics::osc::Argument& arg) {
    std::visit([](const auto& value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, acoustics::osc::Blob>) {
            std::cout << "<blob:" << value.size() << ">";
        } else if constexpr (std::is_same_v<T, bool>) {
            std::cout << (value ? "true" : "false");
        } else {
            std::cout << value;
        }
    }, arg);
}

void printBundle(const ScheduledBundle& bundle) {
    auto tt = std::chrono::system_clock::to_time_t(bundle.executionTime);
    auto localTm = *std::localtime(&tt);
    std::cout << "Bundle @ " << std::put_time(&localTm, "%Y-%m-%d %H:%M:%S")
              << " (messages=" << bundle.messages.size() << ")\n";

    for (const auto& msg : bundle.messages) {
        std::cout << "  " << msg.message.address
                  << " target=" << (msg.targetId ? *msg.targetId : "<broadcast>")
                  << " preset=" << (msg.presetId ? *msg.presetId : "-")
                  << " args=[";
        for (std::size_t i = 0; i < msg.message.arguments.size(); ++i) {
            printArgument(msg.message.arguments[i]);
            if (i + 1 < msg.message.arguments.size()) {
                std::cout << ", ";
            }
        }
        std::cout << "]\n";
    }
}

void sendBundles(const std::vector<ScheduledBundle>& bundles,
                 const SchedulerOptions& options) {
    asio::io_context ioContext;
    asio::ip::address address;
    try {
        address = asio::ip::make_address(options.host);
    } catch (const std::exception& ex) {
        throw std::runtime_error("Invalid destination address: " + options.host + " (" + ex.what() + ")");
    }

    acoustics::osc::OscSender sender(
        ioContext,
        acoustics::osc::OscSender::Endpoint(address, options.port),
        options.broadcast);

    if (options.spacing < 0.01) {
        throw std::runtime_error("--bundle-spacing must be at least 0.01 seconds");
    }

    for (std::size_t i = 0; i < bundles.size(); ++i) {
        sender.send(bundles[i].toOscBundle());
        if (options.spacing > 0.0 && i + 1 < bundles.size()) {
            std::this_thread::sleep_for(std::chrono::duration<double>(options.spacing));
        }
    }
}

}  // namespace

int main(int argc, char** argv) {
    CLI::App app{"Agent A Timeline Scheduler"};
    SchedulerOptions opts;

    app.add_option("timeline", opts.timelinePath, "Timeline JSON file")
        ->required()
        ->check(CLI::ExistingFile);
    app.add_option("--host", opts.host, "Destination host (IPv4)");
    app.add_option("--port", opts.port, "Destination port");
    app.add_option("--lead-time", opts.leadTime, "Override lead time in seconds");
    app.add_option("--bundle-spacing", opts.spacing, "Delay between bundle sends (seconds)");
    app.add_option("--target-map", opts.targetMapPath, "Logical-to-device mapping file (JSON or CSV)");
    app.add_option("--default-targets", opts.defaultTargets, "Fallback device IDs when events omit targets")
        ->delimiter(',')
        ->take_last();
    bool disableBroadcast = false;
    app.add_flag("--no-broadcast", disableBroadcast, "Disable broadcast socket option");
    app.add_flag("--dry-run", opts.dryRun, "Print bundles instead of sending");
    app.add_option("--base-time", opts.baseTimeIso, "Base ISO8601 time (default: now)");

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& e) {
        return app.exit(e);
    }

    opts.broadcast = !disableBroadcast;

    try {
        auto timeline = acoustics::scheduler::SoundTimeline::fromJsonFile(opts.timelinePath);
        if (opts.leadTime >= 0.0 && opts.leadTime < 3.0) {
            throw std::runtime_error("Override lead time must be at least 3 seconds");
        }

        auto baseTime = parseIsoTime(opts.baseTimeIso);
        auto resolver = buildResolver(opts);
        auto scheduled = timeline.schedule(
            baseTime,
            opts.leadTime >= 0.0 ? opts.leadTime : timeline.defaultLeadTimeSeconds(),
            resolver);

        if (opts.dryRun) {
            std::cout << "DRY RUN: Generated " << scheduled.size() << " bundle(s)\n";
            for (const auto& bundle : scheduled) {
                printBundle(bundle);
            }
            return 0;
        }

        sendBundles(scheduled, opts);
        std::cout << "Sent " << scheduled.size() << " bundle(s) to " << opts.host << ":" << opts.port << "\n";
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
