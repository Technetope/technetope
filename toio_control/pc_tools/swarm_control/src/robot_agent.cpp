#include "robot_agent.hpp"
#include "spatial_density_grid.hpp"
#include "cluster_detector.hpp"
#include "predictive_avoidance.hpp"
#include "utils.hpp"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <limits>

namespace swarm_control {

RobotAgent::RobotAgent(double x, double y, int index, 
                       SpeedCategory category, bool isLowHomeostasis)
    : x_(x), y_(y), index_(index), 
      speedCategory_(category),
      isLowHomeostasis_(isLowHomeostasis),
      rng_(static_cast<unsigned int>(index)) {
    
    // 速度カテゴリに基づいて基本速度を設定
    if (speedCategory_ == SpeedCategory::FAST) {
        basePreferredSpeed_ = params::g_params.autonomy.maxSpeed * (1.3 + rng_.random() * 0.4);
    } else if (speedCategory_ == SpeedCategory::MODERATE) {
        const double moderateMin = params::g_params.autonomy.minSpeed + 
                                   (params::g_params.autonomy.maxSpeed - params::g_params.autonomy.minSpeed) * 0.3;
        const double moderateMax = params::g_params.autonomy.maxSpeed * 0.9;
        basePreferredSpeed_ = moderateMin + rng_.random() * (moderateMax - moderateMin);
    } else {
        basePreferredSpeed_ = params::g_params.autonomy.minSpeed * (0.7 + rng_.random() * 0.3);
    }
    
    speedBias_ = 0.8 + rng_.random() * 0.4;
    preferredSpeed_ = basePreferredSpeed_ * speedBias_;
    
    // 動的速度変化用の状態
    speedLevel_ = rng_.randomInt(0, 3);
    speedChangeInterval_ = 1.5 + rng_.random() * 2.5;
    speedChangeTimer_ = rng_.random() * speedChangeInterval_;
    baseSpeed_ = preferredSpeed_;
    speedVariation_ = 0.1 + rng_.random() * 0.2;
    
    // 恒常性
    homeostasisEnergy_ = isLowHomeostasis_ ? 
        (0.2 + rng_.random() * 0.3) : (0.5 + rng_.random() * 0.5);
    homeostasisTarget_ = isLowHomeostasis_ ?
        (0.3 + rng_.random() * 0.2) : (0.6 + rng_.random() * 0.3);
    homeostasisDecay_ = isLowHomeostasis_ ?
        (0.01 + rng_.random() * 0.01) : (0.005 + rng_.random() * 0.01);
    homeostasisThreshold_ = isLowHomeostasis_ ?
        (0.1 + rng_.random() * 0.1) : (0.2 + rng_.random() * 0.2);
    
    // 位置履歴
    stationaryTime_ = 0.0;
    forceMoveTimer_ = 0.0;
    lastPositionX_ = x;
    lastPositionY_ = y;
    
    // 動的パラメータ
    dynamicParameterInterval_ = 2.0 + rng_.random() * 3.0;
    parameterVariation_ = 0.1 + rng_.random() * 0.2;
    
    // 向きベースの移動
    targetOrientation_ = angle_;
    maxOrientationSpeed_ = (M_PI / 6) + rng_.random() * (M_PI / 6);
    
    // 加速度・減速度
    accelerationRate_ = 40.0 + rng_.random() * 50.0;
    decelerationRate_ = 50.0 + rng_.random() * 60.0;
    currentSpeed_ = 0.0;
    
    // 2輪ロボット
    wheelBase_ = 70.0;
    maxWheelSpeed_ = params::g_params.autonomy.maxSpeed * 1.5;
    
    // 自律性
    currentHeading_ = rng_.random() * 2.0 * M_PI;
    headingPersistence_ = params::g_params.autonomy.persistenceTime * (1.0 + rng_.random());
    sharpTurnTimer_ = 3.0 + rng_.random() * 4.0;
    explorationDirection_ = rng_.random() * 2.0 * M_PI;
    explorationTimer_ = params::g_params.autonomy.randomWalkInterval * 0.5 + 
                       rng_.random() * params::g_params.autonomy.randomWalkInterval * 0.5;
    
    // 初期速度
    const double initialAngle = rng_.random() * 2.0 * M_PI;
    vx_ = std::cos(initialAngle) * preferredSpeed_;
    vy_ = std::sin(initialAngle) * preferredSpeed_;
}

// assignSpeedCategoryメソッドは削除（DeviceManagerで割り当て済み）

DifferentialDrive RobotAgent::computeDifferentialDrive(double linearVel, double angularVel) {
    leftWheelSpeed_ = linearVel - (angularVel * wheelBase_) / 2.0;
    rightWheelSpeed_ = linearVel + (angularVel * wheelBase_) / 2.0;
    
    // 最大速度で制限
    const double maxSpeed = maxWheelSpeed_;
    leftWheelSpeed_ = std::max(-maxSpeed, std::min(maxSpeed, leftWheelSpeed_));
    rightWheelSpeed_ = std::max(-maxSpeed, std::min(maxSpeed, rightWheelSpeed_));
    
    // 実際の線速度と角速度を再計算
    DifferentialDrive drive;
    drive.linearVelocity = (leftWheelSpeed_ + rightWheelSpeed_) / 2.0;
    drive.angularVelocity = (rightWheelSpeed_ - leftWheelSpeed_) / wheelBase_;
    
    return drive;
}

Velocity RobotAgent::projectVelocityToOrientation(double vx, double vy) {
    const double forwardX = std::cos(angle_);
    const double forwardY = std::sin(angle_);
    
    const double projectedSpeed = vx * forwardX + vy * forwardY;
    
    Velocity vel;
    vel.vx = forwardX * projectedSpeed;
    vel.vy = forwardY * projectedSpeed;
    
    return vel;
}

void RobotAgent::applyMinimalShaking(double dt) {
    // 最小限の振動を追加
    const double shakeAmount = 0.1 + rng_.random() * 0.2;
    x_ += (rng_.random() - 0.5) * shakeAmount * dt;
    y_ += (rng_.random() - 0.5) * shakeAmount * dt;
}

void RobotAgent::update(double dt) {
    // 速度を向きに投影
    const auto projected = projectVelocityToOrientation(vx_, vy_);
    
    // 速度の平滑化
    const double smoothing = 0.3 + rng_.random() * 0.3;
    smoothedVx_ = smoothing * smoothedVx_ + (1.0 - smoothing) * projected.vx;
    smoothedVy_ = smoothing * smoothedVy_ + (1.0 - smoothing) * projected.vy;
    
    // 位置を更新
    x_ += smoothedVx_ * dt;
    y_ += smoothedVy_ * dt;
    
    // 最小限の振動を適用
    applyMinimalShaking(dt);
    
    // 角度を更新（差動駆動から）
    const double speed = std::hypot(smoothedVx_, smoothedVy_);
    if (speed > 1.0) {
        const double targetAngle = std::atan2(smoothedVy_, smoothedVx_);
        double angleDiff = utils::angleDifference(angle_, targetAngle);
        
        const double angularVel = angleDiff / dt;
        const auto drive = computeDifferentialDrive(speed, angularVel);
        
        angle_ += drive.angularVelocity * dt;
        angle_ = utils::normalizeAngle(angle_);
    }
}

void RobotAgent::updateDynamicSpeed(double dt) {
    speedChangeTimer_ -= dt;
    if (speedChangeTimer_ <= 0.0) {
        speedLevel_ = rng_.randomInt(0, 3);
        speedChangeTimer_ = speedChangeInterval_;
        
        if (speedLevel_ == 0) {
            baseSpeed_ = params::g_params.autonomy.minSpeed * (0.5 + rng_.random() * 0.3);
        } else if (speedLevel_ == 1) {
            baseSpeed_ = params::g_params.autonomy.minSpeed + 
                        (params::g_params.autonomy.maxSpeed - params::g_params.autonomy.minSpeed) * 
                        (0.3 + rng_.random() * 0.4);
        } else {
            baseSpeed_ = params::g_params.autonomy.maxSpeed * (0.7 + rng_.random() * 0.3);
        }
    }
    
    // 個体差を適用
    const double individualVariation = 1.0 + (rng_.random() - 0.5) * speedVariation_;
    const double targetSpeed = baseSpeed_ * individualVariation;
    
    // 加速度/減速度で更新
    if (targetSpeed > currentSpeed_) {
        currentSpeed_ = std::min(targetSpeed, currentSpeed_ + accelerationRate_ * dt);
    } else {
        currentSpeed_ = std::max(targetSpeed, currentSpeed_ - decelerationRate_ * dt);
    }
    
    preferredSpeed_ = currentSpeed_;
}

void RobotAgent::updateHomeostasis(double dt, 
                                   const std::vector<RobotAgent*>& neighbors) {
    // 位置履歴を更新
    PositionHistory hist;
    hist.x = x_;
    hist.y = y_;
    hist.time = std::chrono::steady_clock::now();
    positionHistory_.push_back(hist);
    
    if (positionHistory_.size() > static_cast<size_t>(params::g_params.autonomy.positionHistorySize)) {
        positionHistory_.erase(positionHistory_.begin());
    }
    
    // 50cm以内のロボットとの距離をチェック
    bool tooClose = false;
    double closestDistance = std::numeric_limits<double>::max();
    for (const auto* neighbor : neighbors) {
        auto pos = neighbor->getPosition();
        const double dist = utils::distance(x_, y_, pos.x, pos.y);
        if (dist < 500.0) {
            tooClose = true;
            closestDistance = std::min(closestDistance, dist);
        }
    }
    
    if (tooClose && closestDistance < 500.0) {
        forceMoveTimer_ = 1.0 + rng_.random() * 2.0;
        homeostasisEnergy_ = std::max(0.1, homeostasisEnergy_ - 0.1);
    }
    
    // 静止検出
    if (positionHistory_.size() >= static_cast<size_t>(params::g_params.autonomy.positionHistorySize)) {
        const auto& oldest = positionHistory_[0];
        const auto& newest = positionHistory_.back();
        const auto timeDiff = std::chrono::duration<double>(
            newest.time - oldest.time).count();
        
        if (timeDiff > 0.1) {
            const double distance = utils::distance(oldest.x, oldest.y, newest.x, newest.y);
            const double avgSpeed = distance / timeDiff;
            
            if (avgSpeed < params::g_params.autonomy.stationaryThreshold) {
                stationaryTime_ += dt;
                
                if (stationaryTime_ > params::g_params.autonomy.stationaryTimeThreshold) {
                    forceMoveTimer_ = 2.0 + rng_.random() * 2.0;
                    stationaryTime_ = 0.0;
                    currentHeading_ = rng_.random() * 2.0 * M_PI;
                    homeostasisEnergy_ = 0.1;
                }
            } else {
                stationaryTime_ = 0.0;
            }
        }
    }
    
    // エネルギーを減衰
    homeostasisEnergy_ -= homeostasisDecay_ * dt;
    
    // 強制移動中はエネルギーを回復
    if (forceMoveTimer_ > 0.0) {
        forceMoveTimer_ -= dt;
        const double recoveryRate = 0.02 + rng_.random() * 0.03;
        homeostasisEnergy_ += recoveryRate * dt;
    }
    
    // エネルギーの上限チェック
    if (homeostasisEnergy_ > utils::MAX_HOMEOSTASIS_ENERGY) {
        homeostasisEnergy_ = utils::MAX_HOMEOSTASIS_ENERGY;
    }
    
    // エネルギーの下限チェック（既存の処理で0.1以上に保たれているが、念のため）
    if (homeostasisEnergy_ < 0.0) {
        homeostasisEnergy_ = 0.0;
    }
}

Position RobotAgent::findPathAroundHumanFeet(
    const std::vector<HumanSpot>& humanSpots,
    const std::vector<RobotAgent*>& robots) {
    (void)robots;  // 現状はロボット一覧を使用しない
    
    // 簡易実装：人間の足を避ける
    Position target;
    target.x = x_;
    target.y = y_;
    
    for (const auto& spot : humanSpots) {
        const double dist = utils::distance(x_, y_, spot.headX, spot.headY);
        if (dist < 50.0) { // 0.05m以内
            // 人間から離れる方向
            const double dx = x_ - spot.headX;
            const double dy = y_ - spot.headY;
            const double angle = std::atan2(dy, dx);
            target.x = x_ + std::cos(angle) * 200.0;
            target.y = y_ + std::sin(angle) * 200.0;
            break;
        }
    }
    
    return target;
}

void RobotAgent::updateAutonomy(double dt,
                                const std::vector<HumanSpot>& humanSpots,
                                const std::vector<RobotAgent*>& robots) {
    // 局所最小値検出
    const double movedDist = utils::distance(x_, y_, lastPositionX_, lastPositionY_);
    if (movedDist < 10.0) {
        stuckTimer_ += dt;
    } else {
        stuckTimer_ = 0.0;
    }
    
    if (stuckTimer_ > stuckThreshold_) {
        // 脱出モード
        currentHeading_ = rng_.random() * 2.0 * M_PI;
        stuckTimer_ = 0.0;
    }
    
    lastPositionX_ = x_;
    lastPositionY_ = y_;
    
    // 急旋回チェック
    sharpTurnTimer_ -= dt;
    if (sharpTurnTimer_ <= 0.0) {
        if (rng_.random() < params::g_params.autonomy.sharpTurnProbability) {
            const double turnAngle = (rng_.random() - 0.5) * params::g_params.autonomy.sharpTurnAngle;
            currentHeading_ += turnAngle;
        }
        sharpTurnTimer_ = 3.0 + rng_.random() * 4.0;
    }
    
    // 相関ランダムウォーク
    headingPersistence_ -= dt;
    if (headingPersistence_ <= 0.0) {
        const double angleChange = (rng_.random() - 0.5) * params::g_params.autonomy.persistenceAngle;
        currentHeading_ += angleChange;
        headingPersistence_ = params::g_params.autonomy.persistenceTime * (1.0 + rng_.random());
    }
    
    // 人間の足の周りを探索
    if (isLowHomeostasis_ && rng_.random() < 0.3) {
        const auto target = findPathAroundHumanFeet(humanSpots, robots);
        const double dx = target.x - x_;
        const double dy = target.y - y_;
        if (std::hypot(dx, dy) > 1.0) {
            currentHeading_ = std::atan2(dy, dx);
        }
    }
    
    // 目標速度を計算
    const double speed = preferredSpeed_;
    vx_ = std::cos(currentHeading_) * speed;
    vy_ = std::sin(currentHeading_) * speed;
}

void RobotAgent::applyCollisionAvoidance(const std::vector<RobotAgent*>& otherRobots,
                                         double dt,
                                         SpatialDensityGrid* densityGrid,
                                         PredictiveAvoidance* predictiveAvoidance) {
    (void)dt;  // 現在のロジックでは時間ステップを使用しない
    double avoidanceX = 0.0, avoidanceY = 0.0;
    
    // 能動的ロボット（FAST）は混雑度回避と予測回避をスキップ
    if (speedCategory_ != SpeedCategory::FAST) {
        // 混雑度回避
        if (densityGrid) {
            const auto leastCrowded = densityGrid->getLeastCrowdedDirection(x_, y_);
            if (leastCrowded.has_value()) {
                const double gain = params::g_params.densityGrid.repulsionGain;
                avoidanceX += std::cos(leastCrowded->angle) * gain;
                avoidanceY += std::sin(leastCrowded->angle) * gain;
            }
        }
        
        // 予測回避
        if (predictiveAvoidance) {
            const auto predictive = predictiveAvoidance->applyPredictiveAvoidance(this, otherRobots);
            avoidanceX += predictive.fx;
            avoidanceY += predictive.fy;
        }
    }
    
    // 衝突回避
    for (const auto* other : otherRobots) {
        if (other == this) continue;
        
        auto pos = other->getPosition();
        const double dx = x_ - pos.x;
        const double dy = y_ - pos.y;
        const double dist = utils::distance(x_, y_, pos.x, pos.y);
        
        if (dist < params::g_params.collision.emergencyStopDistance) {
            // 緊急停止
            vx_ *= params::g_params.collision.emergencyStopSpeed;
            vy_ *= params::g_params.collision.emergencyStopSpeed;
        } else if (dist < params::g_params.collision.minSeparation) {
            // 分離
            if (dist > utils::EPSILON_DISTANCE) {  // ゼロ除算防止
                const double force = params::g_params.collision.separationForce;
                const double strength = force / (dist * dist + 1.0);
                avoidanceX += (dx / dist) * strength;
                avoidanceY += (dy / dist) * strength;
            }
        } else if (dist < params::g_params.collision.safeDistance) {
            // 反発
            if (dist > utils::EPSILON_DISTANCE) {  // ゼロ除算防止
                const double force = params::g_params.collision.repulsionGain;
                const double strength = force / (dist * dist + 1.0);
                avoidanceX += (dx / dist) * strength;
                avoidanceY += (dy / dist) * strength;
            }
        }
    }
    
    // 重み付けして合成（現状は衝突回避重みのみ使用）
    const double weightCollision = params::g_params.densityGrid.weightCollision;
    vx_ += avoidanceX * weightCollision;
    vy_ += avoidanceY * weightCollision;
}

void RobotAgent::applyCollisionBrake(const std::vector<RobotAgent*>& otherRobots) {
    for (const auto* other : otherRobots) {
        if (other == this) continue;
        
        auto pos = other->getPosition();
        const double dist = utils::distance(x_, y_, pos.x, pos.y);
        
        if (dist < params::g_params.collision.stopDistance) {
            const double brakeFactor = dist / params::g_params.collision.stopDistance;
            vx_ *= brakeFactor;
            vy_ *= brakeFactor;
        }
    }
}

void RobotAgent::applyBoundaryReflection() {
    const double margin = 60.0;
    const double damping = 0.5;
    
    // X方向の境界処理：速度を適切に反転
    if (x_ < params::g_params.field.minX + margin) {
        if (vx_ < 0.0) {  // 左方向に進んでいる場合のみ反転
            vx_ = -vx_ * damping;
        }
    } else if (x_ > params::g_params.field.maxX - margin) {
        if (vx_ > 0.0) {  // 右方向に進んでいる場合のみ反転
            vx_ = -vx_ * damping;
        }
    }
    
    // Y方向の境界処理：速度を適切に反転
    if (y_ < params::g_params.field.minY + margin) {
        if (vy_ < 0.0) {  // 下方向に進んでいる場合のみ反転
            vy_ = -vy_ * damping;
        }
    } else if (y_ > params::g_params.field.maxY - margin) {
        if (vy_ > 0.0) {  // 上方向に進んでいる場合のみ反転
            vy_ = -vy_ * damping;
        }
    }
}

void RobotAgent::applySpeedLimit() {
    const double speed = std::hypot(vx_, vy_);
    const double maxSpeed = params::g_params.robot.maxSpeed;
    
    if (speed > maxSpeed && speed > utils::EPSILON_DISTANCE) {  // ゼロ除算防止
        vx_ = (vx_ / speed) * maxSpeed;
        vy_ = (vy_ / speed) * maxSpeed;
    }
    
    // 最小速度を確保
    const double minSpeed = params::g_params.autonomy.minSpeed * 0.5;
    if (speed < minSpeed && speed > 0.1 && speed > utils::EPSILON_DISTANCE) {  // ゼロ除算防止
        vx_ = (vx_ / speed) * minSpeed;
        vy_ = (vy_ / speed) * minSpeed;
    }
}

void RobotAgent::clampPosition() {
    x_ = std::max(params::g_params.field.minX, 
                  std::min(params::g_params.field.maxX, x_));
    y_ = std::max(params::g_params.field.minY, 
                  std::min(params::g_params.field.maxY, y_));
}

std::vector<RobotAgent*> RobotAgent::getNeighborsWithinDistance(
    const std::vector<RobotAgent*>& robots, double radius) {
    
    std::vector<RobotAgent*> neighbors;
    const double radiusSq = radius * radius;
    
    for (auto* robot : robots) {
        if (robot == this) continue;
        auto pos = robot->getPosition();
        const double dx = x_ - pos.x;
        const double dy = y_ - pos.y;
        const double distSq = dx * dx + dy * dy;
        if (distSq < radiusSq) {
            neighbors.push_back(robot);
        }
    }
    
    return neighbors;
}

}  // namespace swarm_control
