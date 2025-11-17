#pragma once

#include <map>
#include <set>
#include <chrono>
#include <optional>
#include "params.hpp"
#include "human_spot.hpp"
#include "velocity.hpp"

namespace swarm_control {

// 前方宣言
class RobotAgent;

struct UrgentState {
    bool active = false;
    double escapeAngle = 0.0;
    std::chrono::steady_clock::time_point startTime;
};

class UrgentEscape {
public:
    UrgentEscape();
    
    // 緊急退避をチェック
    void checkUrgentEscape(std::vector<RobotAgent*>& robots,
                           const std::vector<HumanSpot>& spots);
    
    // ロボットが緊急退避モードかチェック
    bool isUrgent(int robotIndex) const;
    
    // 緊急退避の速度を取得
    std::optional<Velocity> computeUrgentVelocity(int robotIndex) const;

private:
    // 緊急退避角度を計算
    double computeEscapeAngle(const RobotAgent* robot, 
                             const HumanSpot& spot) const;
    
    std::map<int, UrgentState> urgentStates_;  // ロボットインデックス -> 状態
    std::set<int> affectedRobots_;            // 緊急退避中のロボット
    std::chrono::steady_clock::time_point currentTime_;
};

}  // namespace swarm_control
