#include "acoustics/scheduler/SoundTimeline.h"
#include "acoustics/scheduler/TargetResolver.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

std::filesystem::path writeTimeline(const std::string& content) {
    static std::size_t counter = 0;
    const auto suffix = std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count() + counter++);
    const auto tempPath = std::filesystem::temp_directory_path()
                          / ("timeline-" + suffix + ".json");
    std::ofstream output(tempPath);
    REQUIRE(output.good());
    output << content;
    output.close();
    return tempPath;
}

}  // namespace

TEST_CASE("SoundTimeline enforces minimum lead time", "[scheduler]") {
    const std::string timelineJson = R"({
        "version": "1.1",
        "default_lead_time": 3.2,
        "events": [
            { "offset": 0.0, "address": "/acoustics/play", "args": ["voice_a", 0, 1.0, 0] }
        ]
    })";

    const auto path = writeTimeline(timelineJson);
    auto timeline = acoustics::scheduler::SoundTimeline::fromJsonFile(path);
    std::filesystem::remove(path);

    acoustics::scheduler::TargetResolver resolver;
    auto baseTime = std::chrono::system_clock::from_time_t(1'700'000'000);

    SECTION("timeline default lead time accepted") {
        REQUIRE_NOTHROW(timeline.schedule(baseTime, -1.0, resolver));
    }

    SECTION("override lead time below minimum is rejected") {
        REQUIRE_THROWS_AS(timeline.schedule(baseTime, 2.5, resolver), std::runtime_error);
    }
}

TEST_CASE("SoundTimeline expands targets via resolver", "[scheduler]") {
    const std::string timelineJson = R"({
        "version": "1.1",
        "default_lead_time": 3.5,
        "events": [
            { "offset": 0.0, "address": "/acoustics/play", "args": ["round_intro", 0, 1.0, 0] },
            { "offset": 0.5, "address": "/acoustics/play", "targets": ["voice_a", "voice_b"],
              "args": ["round_phrase", 0, 0.8, 0] }
        ]
    })";

    const auto path = writeTimeline(timelineJson);
    auto timeline = acoustics::scheduler::SoundTimeline::fromJsonFile(path);
    std::filesystem::remove(path);

    acoustics::scheduler::TargetResolver resolver;
    resolver.setMapping({
        {"voice_a", {"dev-001"}},
        {"voice_b", {"dev-010", "dev-011"}}
    });

    const auto baseTime = std::chrono::system_clock::from_time_t(1'700'000'100);
    const auto bundles = timeline.schedule(baseTime, -1.0, resolver);

    REQUIRE(bundles.size() == 2);

    const auto& introBundle = bundles.front();
    REQUIRE(introBundle.messages.size() == 3);
    std::vector<std::string> introTargets;
    for (const auto& msg : introBundle.messages) {
        REQUIRE(msg.targetId.has_value());
        introTargets.push_back(*msg.targetId);
        REQUIRE(msg.presetId.has_value());
        CHECK(msg.presetId.value() == "round_intro");
    }
    std::sort(introTargets.begin(), introTargets.end());
    CHECK(introTargets == std::vector<std::string>{"dev-001", "dev-010", "dev-011"});

    const auto& phraseBundle = bundles.back();
    REQUIRE(phraseBundle.messages.size() == 3);  // voice_a -> 1 device, voice_b -> 2
    std::vector<std::string> collectedTargets;
    for (const auto& msg : phraseBundle.messages) {
        REQUIRE(msg.targetId.has_value());
        collectedTargets.push_back(*msg.targetId);
        REQUIRE(msg.presetId.has_value());
        CHECK(msg.presetId.value() == "round_phrase");
    }

    std::sort(collectedTargets.begin(), collectedTargets.end());
    CHECK(collectedTargets == std::vector<std::string>{"dev-001", "dev-010", "dev-011"});
}
