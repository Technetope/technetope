#include "urgent_escape.hpp"
#include "robot_agent.hpp"
#include "utils.hpp"
#include <cmath>

namespace swarm_control {

UrgentEscape::UrgentEscape() {
    currentTime_ = std::chrono::steady_clock::now();
}

void UrgentEscape::checkUrgentEscape(std::vector<RobotAgent*>& robots,
                                     const std::vector<HumanSpot>& spots) {
    currentTime_ = std::chrono::steady_clock::now();
    affectedRobots_.clear();
    
    for (const auto& spot : spots) {
        if (!spot.urgent) continue;
        
        for (size_t i = 0; i < robots.size(); ++i) {
            auto* robot = robots[i];
            auto pos = robot->getPosition();
            
            const double dx = pos.x - spot.headX;
            const double dy = pos.y - spot.headY;
            const double dist = utils::distance(pos.x, pos.y, spot.headX, spot.headY);
            
            // 閾値を広げる（反応を早く、2倍の範囲で反応）
            if (dist < params::g_params.urgent.threshold * 2.0) {
                // 緊急退避をアクティブ化
                UrgentState state;
                state.active = true;
                state.escapeAngle = computeEscapeAngle(robot, spot);
                state.startTime = currentTime_;
                
                urgentStates_[static_cast<int>(i)] = state;
                affectedRobots_.insert(static_cast<int>(i));
            }
        }
    }
    
    // 緊急退避の時間制限チェック
    const double durationMs = params::g_params.urgent.duration;
    for (auto it = urgentStates_.begin(); it != urgentStates_.end();) {
        if (it->second.active) {
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                currentTime_ - it->second.startTime).count();
            
            if (elapsed > durationMs) {
                it->second.active = false;
                affectedRobots_.erase(it->first);
                it = urgentStates_.erase(it);
            } else {
                ++it;
            }
        } else {
            ++it;
        }
    }
}

double UrgentEscape::computeEscapeAngle(const RobotAgent* robot, 
                                        const HumanSpot& spot) const {
    auto pos = robot->getPosition();
    
    // ロボットから脅威へのベクトル
    const double dx = spot.headX - pos.x;
    const double dy = spot.headY - pos.y;
    
    // その逆方向（180度回転）が逃げる方向
    double escapeAngle = std::atan2(-dy, -dx);
    
    // 脅威の速度も考慮（予測回避）
    const double threatSpeed = utils::distance(0.0, 0.0, spot.velocityX, spot.velocityY);
    if (threatSpeed > params::g_params.urgent.threatSpeedThreshold) {
        // 脅威の進行方向を考慮して、より安全な方向に調整
        const double threatHeading = std::atan2(spot.velocityY, spot.velocityX);
        escapeAngle = threatHeading + M_PI + 
                     0.3 * (escapeAngle - threatHeading - M_PI);
    }
    
    return escapeAngle;
}

bool UrgentEscape::isUrgent(int robotIndex) const {
    auto it = urgentStates_.find(robotIndex);
    return it != urgentStates_.end() && it->second.active;
}

std::optional<UrgentEscape::Velocity> UrgentEscape::computeUrgentVelocity(int robotIndex) const {
    auto it = urgentStates_.find(robotIndex);
    if (it == urgentStates_.end() || !it->second.active) {
        return std::nullopt;
    }
    
    Velocity vel;
    const double escapeSpeed = params::g_params.urgent.escapeSpeed;
    vel.vx = escapeSpeed * std::cos(it->second.escapeAngle);
    vel.vy = escapeSpeed * std::sin(it->second.escapeAngle);
    
    return vel;
}

}  // namespace swarm_control

