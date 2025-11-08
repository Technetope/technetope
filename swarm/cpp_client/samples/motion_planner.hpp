#pragma once

#include "toio/middleware/cube_state.hpp"

#include <chrono>
#include <vector>

namespace swarm::samples {

struct TargetPoint {
  double x = 0.0;
  double y = 0.0;
};

struct MotionPlannerParameters {
  double center_x = 250.0;
  double center_y = 250.0;
  double base_radius = 120.0;
  double radius_amplitude = 30.0;
  double angular_speed = 0.45;            // rad/s
  double radius_oscillation_period = 14.0; // seconds
  double direction_flip_interval = 6.0;    // seconds
};

class MotionPlanner {
public:
  explicit MotionPlanner(MotionPlannerParameters params = {});

  std::vector<TargetPoint> initial_targets(std::size_t cube_count) const;
  std::vector<TargetPoint>
  next_targets(const std::vector<toio::middleware::Position> &positions);

private:
  double oscillating_radius(double elapsed_seconds) const;

  MotionPlannerParameters params_;
  std::chrono::steady_clock::time_point start_time_;
  std::chrono::steady_clock::time_point last_time_;
  double base_angle_;
  double direction_;
  double direction_timer_;
};

} // namespace swarm::samples
