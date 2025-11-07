#include "fleet_orchestrator.hpp"

#include <stdexcept>
#include <string>

#include "util/logging.hpp"

namespace toio::control {

FleetOrchestrator::FleetOrchestrator(CubeRegistry& registry, RelayManager& relay_manager,
                                     const MotionController& motion_controller)
    : registry_(registry), relay_manager_(relay_manager), motion_controller_(motion_controller) {}

std::string FleetOrchestrator::assign_goal(const GoalRequest& request) {
    if (request.targets.empty()) {
        throw std::invalid_argument("GoalRequest.targets must not be empty");
    }

    const auto counter = ++goal_counter_;
    std::string goal_id = "goal-" + std::to_string(counter);
    const auto now = std::chrono::system_clock::now();
    GoalAssignment assignment{.goal_id = goal_id,
                              .cube_id = request.targets.front(),
                              .pose = request.pose,
                              .priority = request.priority,
                              .created_at = now};

    TrackedGoal tracked;
    tracked.assignment = assignment;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        active_goals_[assignment.cube_id] = std::move(tracked);
        if (request.keep_history) {
            history_.push_back(assignment);
            if (history_.size() > max_history_) {
                history_.pop_front();
            }
        }
    }

    util::log::info("Assigned goal " + goal_id + " to cube " + assignment.cube_id);
    return goal_id;
}

void FleetOrchestrator::clear_goal(const std::string& cube_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    active_goals_.erase(cube_id);
}

FleetOrchestrator::FleetState FleetOrchestrator::snapshot() const {
    FleetState state;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        state.tasks_in_queue = active_goals_.size();
        state.active_goals.reserve(active_goals_.size());
        for (const auto& [cube_id, tracked] : active_goals_) {
            (void)cube_id;
            state.active_goals.push_back(tracked.assignment);
        }
    }

    const auto cubes = registry_.snapshot();
    for (const auto& cube : cubes) {
        if (!cube.has_position) {
            state.warnings.push_back("Cube " + cube.cube_id + " position unknown");
        }
    }

    return state;
}

bool FleetOrchestrator::dispatch_command(const std::string& cube_id,
                                         const MotionController::MotionCommand& command) {
    RelayManager::ManualDriveCommand drive;
    drive.targets = {cube_id};
    drive.left = command.left;
    drive.right = command.right;
    std::string error;
    if (!relay_manager_.send_manual_drive(drive, &error)) {
        util::log::warn("Failed manual_drive for cube " + cube_id + ": " + error);
        return false;
    }
    return true;
}

namespace {
constexpr auto kCommandInterval = std::chrono::milliseconds(50);
}

void FleetOrchestrator::tick(std::chrono::steady_clock::time_point now) {
    std::vector<std::string> completed;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& [cube_id, tracked] : active_goals_) {
            auto pose_opt = registry_.pose(cube_id);
            if (!pose_opt) {
                continue;
            }
           auto pose_time = registry_.last_update(cube_id);
           if (pose_time && pose_time <= tracked.last_pose_sample &&
               tracked.last_command.time_since_epoch().count() != 0) {
               continue;
           }
           if (pose_time) {
               tracked.last_pose_sample = *pose_time;
           }

            MotionController::GoalPose goal_pose{
                .x = tracked.assignment.pose.x,
                .y = tracked.assignment.pose.y,
                .angle = tracked.assignment.pose.angle,
            };
            auto decision = motion_controller_.evaluate(*pose_opt, goal_pose);
            if (decision.reached) {
                completed.push_back(cube_id);
                continue;
            }
            if (!decision.has_command) {
                continue;
            }
            if (tracked.last_command.time_since_epoch().count() != 0 &&
                now - tracked.last_command < kCommandInterval) {
                continue;
            }
            if (dispatch_command(cube_id, decision.command)) {
                tracked.last_command = now;
            }
        }
    }

    if (!completed.empty()) {
        const auto cleared_at = std::chrono::system_clock::now();
        for (const auto& cube_id : completed) {
            clear_goal(cube_id);
            CubeRegistry::Update update;
            update.cube_id = cube_id;
            update.goal_id = "";
            update.timestamp = cleared_at;
            registry_.apply_update(update);
            util::log::info("Cube " + cube_id + " reached goal");
        }
    }
}

}  // namespace toio::control
