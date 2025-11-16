#pragma once

#include <vector>
#include "params.hpp"

namespace swarm_control {

// 前方宣言
class RobotAgent;

struct FuturePosition {
    double x = 0.0;           // 将来位置 X (mm)
    double y = 0.0;           // 将来位置 Y (mm)
    double radius = 0.0;      // 予測領域の半径 (mm)
    double time = 0.0;        // 予測時間 (s)
};

struct AvoidanceForce {
    double fx = 0.0;          // 反発力 X (mm/s²)
    double fy = 0.0;          // 反発力 Y (mm/s²)
};

class PredictiveAvoidance {
public:
    PredictiveAvoidance();
    
    // ロボットの将来位置を予測
    FuturePosition predictFuturePosition(
        const RobotAgent* robot, double predictionTime) const;
    
    // 予測回避力を適用
    AvoidanceForce applyPredictiveAvoidance(
        const RobotAgent* robot,
        const std::vector<RobotAgent*>& allRobots) const;

private:
    // 自分の将来位置が他のロボットの予測領域と衝突するかチェック
    AvoidanceForce checkCollisionWithPredictions(
        const RobotAgent* robot,
        const std::vector<RobotAgent*>& allRobots) const;
    
    std::vector<double> predictionTimes_;  // [0.5, 1.0, 2.0]秒
    double predictionRadius_;             // 76mm
    double repulsionGain_;                // 1500.0
    double uncertaintyGrowth_;            // 0.2
};

}  // namespace swarm_control

