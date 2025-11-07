#pragma once

#include <optional>
#include <vector>

#include "cube_registry.hpp"

namespace toio::control {

class MotionController {
public:
    struct MotionCommand {
        int left{0};
        int right{0};
        int duration_ms{0};
    };

    struct ControlDecision {
        bool reached{false};
        MotionCommand command{};
        bool has_command{false};
        double distance_mm{0.0};
        double heading_error_deg{0.0};
    };

    struct GoalPose {
        double x{0.0};
        double y{0.0};
        std::optional<double> angle;
    };

    ControlDecision evaluate(const CubeRegistry::Pose& current, const GoalPose& goal) const;

private:
    static double normalize_angle(double deg);
};

}  // namespace toio::control
