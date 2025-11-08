#include "motion_planner.hpp"

#include <algorithm>
#include <cmath>

namespace {

constexpr double kTwoPi = 2.0 * 3.14159265358979323846;

} // namespace

namespace swarm::samples {

MotionPlanner::MotionPlanner(MotionPlannerParameters params)
    : params_(params),
      start_time_(std::chrono::steady_clock::now()),
      last_time_(start_time_),
      base_angle_(0.0),
      direction_(1.0),
      direction_timer_(0.0) {}

std::vector<TargetPoint>
MotionPlanner::initial_targets(std::size_t cube_count) const {
  std::vector<TargetPoint> targets;
  if (cube_count == 0) {
    return targets;
  }
  targets.reserve(cube_count);
  const double radius = oscillating_radius(0.0);
  const double delta =
      kTwoPi / static_cast<double>(cube_count);
  for (std::size_t i = 0; i < cube_count; ++i) {
    const double angle = delta * static_cast<double>(i);
    TargetPoint point;
    point.x = params_.center_x + radius * std::cos(angle);
    point.y = params_.center_y + radius * std::sin(angle);
    targets.push_back(point);
  }
  return targets;
}

std::vector<TargetPoint> MotionPlanner::next_targets(
    const std::vector<toio::middleware::Position> &positions) {
  std::vector<TargetPoint> targets;
  if (positions.empty()) {
    return targets;
  }

  const auto now = std::chrono::steady_clock::now();
  const double elapsed =
      std::chrono::duration<double>(now - start_time_).count();
  const double dt =
      std::chrono::duration<double>(now - last_time_).count();
  last_time_ = now;

  if (params_.direction_flip_interval > 0.0) {
    direction_timer_ += dt;
    if (direction_timer_ >= params_.direction_flip_interval) {
      direction_timer_ -= params_.direction_flip_interval;
      direction_ = -direction_;
    }
  }

  base_angle_ += direction_ * params_.angular_speed * dt;
  const double radius = oscillating_radius(elapsed);
  const double delta =
      kTwoPi / static_cast<double>(positions.size());

  targets.reserve(positions.size());
  for (std::size_t i = 0; i < positions.size(); ++i) {
    const double angle = base_angle_ + delta * static_cast<double>(i);
    TargetPoint point;
    point.x = params_.center_x + radius * std::cos(angle);
    point.y = params_.center_y + radius * std::sin(angle);
    targets.push_back(point);
  }
  return targets;
}

double MotionPlanner::oscillating_radius(double elapsed_seconds) const {
  if (params_.radius_oscillation_period <= 0.0 ||
      params_.radius_amplitude <= 0.0) {
    return params_.base_radius;
  }
  const double phase =
      kTwoPi * elapsed_seconds / params_.radius_oscillation_period;
  const double radius =
      params_.base_radius + params_.radius_amplitude * std::sin(phase);
  return std::max(30.0, radius);
}

} // namespace swarm::samples
