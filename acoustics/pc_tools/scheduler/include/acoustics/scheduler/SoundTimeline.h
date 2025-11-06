#pragma once

#include "acoustics/osc/OscPacket.h"

#include "acoustics/scheduler/TargetResolver.h"

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace acoustics::scheduler {

struct TimelineEvent {
    double offsetSeconds{};
    std::string address;
    std::vector<osc::Argument> arguments;
    std::vector<std::string> targets;
};

struct ScheduledMessage {
    osc::Message message;
    std::optional<std::string> targetId;
    std::optional<std::string> presetId;
};

struct ScheduledBundle {
    std::chrono::system_clock::time_point executionTime{};
    std::vector<ScheduledMessage> messages;

    osc::Bundle toOscBundle() const;
};

class SoundTimeline {
public:
    static SoundTimeline fromJsonFile(const std::filesystem::path& path);

    const std::vector<TimelineEvent>& events() const noexcept { return events_; }
    double defaultLeadTimeSeconds() const noexcept { return defaultLeadTime_; }
    const std::string& version() const noexcept { return version_; }

    std::vector<ScheduledBundle> schedule(
        std::chrono::system_clock::time_point baseTime,
        double leadTimeSeconds,
        const TargetResolver& resolver
    ) const;

    std::vector<osc::Bundle> toBundles(
        std::chrono::system_clock::time_point baseTime,
        double leadTimeSeconds
    ) const;

private:
    std::string version_{"1.0"};
    double defaultLeadTime_{1.0};
    std::vector<TimelineEvent> events_;
};

}  // namespace acoustics::scheduler
