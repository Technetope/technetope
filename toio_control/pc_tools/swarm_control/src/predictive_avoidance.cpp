#include "predictive_avoidance.hpp"
#include "robot_agent.hpp"
#include "utils.hpp"
#include <cmath>

namespace swarm_control {

PredictiveAvoidance::PredictiveAvoidance() {
    predictionTimes_ = params::g_params.predictive.predictionTimes;
    predictionRadius_ = params::g_params.predictive.predictionRadius;
    repulsionGain_ = params::g_params.predictive.repulsionGain;
    uncertaintyGrowth_ = params::g_params.predictive.uncertaintyGrowth;
}

FuturePosition PredictiveAvoidance::predictFuturePosition(
    const RobotAgent* robot, double predictionTime) const {
    
    auto pos = robot->getPosition();
    auto vel = robot->getVelocity();
    
    // 現在位置 + 速度ベクトル × 予測時間
    FuturePosition future;
    future.x = pos.x + vel.vx * predictionTime;
    future.y = pos.y + vel.vy * predictionTime;
    future.time = predictionTime;
    
    // 予測時間が長いほど、領域の不確実性を大きくする
    const double uncertainty = 1.0 + predictionTime * uncertaintyGrowth_;
    future.radius = predictionRadius_ * uncertainty;
    
    return future;
}

AvoidanceForce PredictiveAvoidance::checkCollisionWithPredictions(
    const RobotAgent* robot,
    const std::vector<RobotAgent*>& allRobots) const {
    
    AvoidanceForce avoidanceForces;
    
    // 自分の将来位置を予測
    std::vector<FuturePosition> myPredictions;
    for (double predTime : predictionTimes_) {
        myPredictions.push_back(predictFuturePosition(robot, predTime));
    }
    
    // 他のロボットの予測領域と衝突チェック
    for (const auto* otherRobot : allRobots) {
        if (otherRobot == robot) continue;
        
        // 各予測時間で衝突チェック
        for (size_t i = 0; i < predictionTimes_.size(); ++i) {
            const FuturePosition& myFuture = myPredictions[i];
            const FuturePosition otherFuture = predictFuturePosition(otherRobot, predictionTimes_[i]);
            
            // 将来位置間の距離
            const double dx = myFuture.x - otherFuture.x;
            const double dy = myFuture.y - otherFuture.y;
            const double dist = utils::distance(myFuture.x, myFuture.y, 
                                                otherFuture.x, otherFuture.y);
            
            // 予測領域が重なる場合（半径の合計より距離が小さい）
            const double combinedRadius = myFuture.radius + otherFuture.radius;
            if (dist < combinedRadius && dist > utils::EPSILON_DISTANCE) {
                // 反発力を計算
                const double timeWeight = 1.0 / (1.0 + predictionTimes_[i]);
                const double overlap = combinedRadius - dist;
                const double strength = repulsionGain_ * timeWeight * (overlap / combinedRadius);
                
                // 反発方向（ゼロ除算防止）
                const double dirX = dx / dist;
                const double dirY = dy / dist;
                
                avoidanceForces.fx += dirX * strength;
                avoidanceForces.fy += dirY * strength;
            }
        }
    }
    
    return avoidanceForces;
}

AvoidanceForce PredictiveAvoidance::applyPredictiveAvoidance(
    const RobotAgent* robot,
    const std::vector<RobotAgent*>& allRobots) const {
    
    return checkCollisionWithPredictions(robot, allRobots);
}

}  // namespace swarm_control

