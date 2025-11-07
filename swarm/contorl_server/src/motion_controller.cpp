#include "motion_controller.hpp"

#include <cmath>

namespace toio::control {

namespace {

constexpr double kTurnSpeed = 60.0;
constexpr double kDriveSpeed = 70.0;
constexpr double kDegreesPerSecond = 180.0;
constexpr double kMillimetersPerSecond = 90.0;
constexpr double kHeadingToleranceDeg = 5.0;
constexpr double kDistanceToleranceMm = 15.0;
constexpr int kMinDurationMs = 40;
constexpr int kMaxDurationMs = 150;

int clamp_motor(int value) {
    if (value > 100) return 100;
    if (value < -100) return -100;
    return value;
}

int duration_from_angle(double degrees) {
    const double abs_deg = std::abs(degrees);
    if (abs_deg < 1e-3) {
        return 0;
    }
    const double seconds = abs_deg / kDegreesPerSecond;
    int duration = static_cast<int>(std::round(seconds * 1000.0));
    if (duration == 0 && abs_deg > kHeadingToleranceDeg) {
        duration = kMinDurationMs;
    }
    return std::clamp(duration, kMinDurationMs, kMaxDurationMs);
}

int duration_from_distance(double mm) {
    const double abs_mm = std::abs(mm);
    if (abs_mm < 1e-3) {
        return 0;
    }
    const double seconds = abs_mm / kMillimetersPerSecond;
    int duration = static_cast<int>(std::round(seconds * 1000.0));
    if (duration == 0 && abs_mm > kDistanceToleranceMm) {
        duration = kMinDurationMs;
    }
    return std::clamp(duration, kMinDurationMs, kMaxDurationMs);
}

}  // namespace

MotionController::ControlDecision MotionController::evaluate(const CubeRegistry::Pose& current,
                                                             const GoalPose& goal) const {
    ControlDecision decision;

    const double dx = goal.x - current.x;
    const double dy = goal.y - current.y;
    const double target_heading = std::atan2(dy, dx) * 180.0 / M_PI;
    const double primary_delta = normalize_angle(target_heading - current.deg);

    const double distance = std::sqrt(dx * dx + dy * dy);
    decision.distance_mm = distance;
    decision.heading_error_deg = primary_delta;

    if (distance <= kDistanceToleranceMm) {
        bool heading_ok = true;
        if (goal.angle) {
            const double final_delta = normalize_angle(*goal.angle - current.deg);
            decision.heading_error_deg = final_delta;
            heading_ok = std::abs(final_delta) <= kHeadingToleranceDeg;
        }
        if (heading_ok) {
            decision.reached = true;
            decision.has_command = false;
            return decision;
        }
    }

    if (std::abs(primary_delta) > kHeadingToleranceDeg) {
        decision.command.left = clamp_motor(primary_delta > 0 ? -kTurnSpeed : kTurnSpeed);
        decision.command.right = clamp_motor(primary_delta > 0 ? kTurnSpeed : -kTurnSpeed);
        decision.command.duration_ms = duration_from_angle(primary_delta);
        decision.has_command = decision.command.duration_ms > 0;
        return decision;
    }

    MotionCommand forward{};
    forward.left = clamp_motor(kDriveSpeed);
    forward.right = clamp_motor(kDriveSpeed);
    forward.duration_ms = duration_from_distance(distance);
    if (forward.duration_ms > 0) {
        decision.command = forward;
        decision.has_command = true;
    }
    return decision;
}

double MotionController::normalize_angle(double deg) {
    double normalized = std::fmod(deg, 360.0);
    if (normalized > 180.0) {
        normalized -= 360.0;
    } else if (normalized < -180.0) {
        normalized += 360.0;
    }
    return normalized;
}

}  // namespace toio::control
