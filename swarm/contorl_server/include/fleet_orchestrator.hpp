#pragma once

#include <atomic>
#include <chrono>
#include <deque>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "cube_registry.hpp"
#include "motion_controller.hpp"
#include "relay_manager.hpp"

namespace toio::control {

class FleetOrchestrator {
public:
    struct GoalPose {
        double x{0.0};
        double y{0.0};
        std::optional<double> angle;
    };

    struct GoalRequest {
        std::vector<std::string> targets;
        GoalPose pose;
        int priority{0};
        bool keep_history{false};
    };

    struct GoalAssignment {
        std::string goal_id;
        std::string cube_id;
        GoalPose pose;
        int priority{0};
        std::chrono::system_clock::time_point created_at{};
    };

    struct FleetState {
        double tick_hz{30.0};
        std::size_t tasks_in_queue{0};
        std::vector<std::string> warnings;
        std::vector<GoalAssignment> active_goals;
    };

    FleetOrchestrator(CubeRegistry& registry, RelayManager& relay_manager, const MotionController& motion_controller);

    std::string assign_goal(const GoalRequest& request);
    void clear_goal(const std::string& cube_id);
    FleetState snapshot() const;
    void tick(std::chrono::steady_clock::time_point now);

private:
    struct TrackedGoal {
        GoalAssignment assignment;
        std::chrono::steady_clock::time_point last_command{};
        std::chrono::system_clock::time_point last_pose_sample{};
    };

    bool dispatch_command(const std::string& cube_id, const MotionController::MotionCommand& command);

    CubeRegistry& registry_;
    RelayManager& relay_manager_;
    const MotionController& motion_controller_;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, TrackedGoal> active_goals_;
    std::deque<GoalAssignment> history_;
    std::size_t max_history_{64};
    std::atomic_uint64_t goal_counter_{0};
};

}  // namespace toio::control
