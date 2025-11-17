#pragma once

#include <vector>
#include <memory>
#include <optional>
#include <chrono>
#include <limits>
#include "params.hpp"
#include "human_spot.hpp"
#include "position.hpp"
#include "velocity.hpp"
#include "utils.hpp"

namespace swarm_control {

// 前方宣言
class SpatialDensityGrid;
class ClusterDetector;
class PredictiveAvoidance;

enum class SpeedCategory {
    FAST,      // 20%: 能動的、回避を無視
    MODERATE,  // 60%: 中程度
    SLOW       // 20%: 遅い
};

struct DifferentialDrive {
    double linearVelocity = 0.0;
    double angularVelocity = 0.0;
};

class RobotAgent {
public:
    RobotAgent(double x, double y, int index, 
               SpeedCategory category, bool isLowHomeostasis);
    
    // 状態更新
    void update(double dt);
    void updateAutonomy(double dt, 
                       const std::vector<HumanSpot>& humanSpots,
                       const std::vector<RobotAgent*>& robots);
    void updateDynamicSpeed(double dt);
    void updateHomeostasis(double dt, 
                          const std::vector<RobotAgent*>& neighbors);
    
    // 衝突回避と相互作用
    void applyCollisionAvoidance(const std::vector<RobotAgent*>& otherRobots,
                                 double dt,
                                 SpatialDensityGrid* densityGrid,
                                 PredictiveAvoidance* predictiveAvoidance);
    void applyCollisionBrake(const std::vector<RobotAgent*>& otherRobots);
    void applyBoundaryReflection();
    void applySpeedLimit();
    void clampPosition();
    
    // ユーティリティ
    SpeedCategory getSpeedCategory() const { return speedCategory_; }
    Position getPosition() const { return {x_, y_}; }
    Velocity getVelocity() const { return {vx_, vy_}; }
    double getAngle() const { return angle_; }
    int getIndex() const { return index_; }
    
    // クラスタ検出用（デッドロック解放）
    void setClusterDetector(ClusterDetector* detector) { clusterDetector_ = detector; }

private:
    // assignSpeedCategoryメソッドは削除（DeviceManagerで割り当て済み）
    // 基本状態
    double x_ = 0.0, y_ = 0.0;           // 位置 (mm)
    double angle_ = 0.0;                  // 角度 (rad)
    double vx_ = 0.0, vy_ = 0.0;         // 速度 (mm/s)
    int index_ = 0;                       // ロボットインデックス
    
    // 速度関連
    SpeedCategory speedCategory_;
    double preferredSpeed_ = 0.0;        // 希望速度 (mm/s)
    double currentSpeed_ = 0.0;          // 現在速度 (mm/s)
    double baseSpeed_ = 0.0;             // 基本速度
    int speedLevel_ = 0;                 // 速度レベル (0, 1, 2)
    double speedChangeTimer_ = 0.0;
    double speedChangeInterval_ = 0.0;
    double speedVariation_ = 0.0;
    double speedBias_ = 0.0;
    double basePreferredSpeed_ = 0.0;
    
    // 2輪ロボットの差動駆動
    double wheelBase_ = 70.0;            // 車輪間距離 (mm)
    double leftWheelSpeed_ = 0.0;        // 左車輪速度 (mm/s)
    double rightWheelSpeed_ = 0.0;      // 右車輪速度 (mm/s)
    double maxWheelSpeed_ = 0.0;        // 最大車輪速度
    
    // 自律性関連
    double currentHeading_ = 0.0;        // 現在の進行方向 (rad)
    double headingPersistence_ = 0.0;    // 方向保持時間
    [[maybe_unused]] double lastHeadingChange_ = 0.0;    // 最後に方向を変えた時刻
    double sharpTurnTimer_ = 0.0;        // 急旋回タイマー
    double explorationDirection_ = 0.0;   // 探索方向 (rad)
    double explorationTimer_ = 0.0;      // 探索タイマー
    
    // 恒常性（homeostasis）
    bool isLowHomeostasis_ = false;
    double homeostasisEnergy_ = 0.0;
    double homeostasisTarget_ = 0.0;
    double homeostasisDecay_ = 0.0;
    double homeostasisThreshold_ = 0.0;
    
    // 位置履歴と静止検出
    struct PositionHistory {
        double x = 0.0;
        double y = 0.0;
        std::chrono::steady_clock::time_point time;
    };
    std::vector<PositionHistory> positionHistory_;
    double lastPositionX_ = 0.0, lastPositionY_ = 0.0;
    double stuckTimer_ = 0.0;
    double stuckThreshold_ = 2.0;
    double stationaryTime_ = 0.0;
    double forceMoveTimer_ = 0.0;
    
    // エントロピー
    [[maybe_unused]] double entropy_ = 0.5;
    std::vector<double> entropyHistory_;
    
    // 動的パラメータ
    [[maybe_unused]] double dynamicParameterTimer_ = 0.0;
    double dynamicParameterInterval_ = 0.0;
    double parameterVariation_ = 0.0;
    
    // 向きベースの移動
    double targetOrientation_ = 0.0;
    [[maybe_unused]] double orientationSpeed_ = 0.0;
    double maxOrientationSpeed_ = 0.0;
    [[maybe_unused]] bool isRotating_ = false;
    
    // 速度の平滑化
    double smoothedVx_ = 0.0, smoothedVy_ = 0.0;
    
    // 加速度・減速度
    double accelerationRate_ = 0.0;
    double decelerationRate_ = 0.0;
    
    // クラスタ検出（デッドロック解放用）
    ClusterDetector* clusterDetector_ = nullptr;
    
    // 乱数生成器
    utils::RandomGenerator rng_;
    
    // 内部メソッド
    // assignSpeedCategoryメソッドは削除（DeviceManagerで割り当て済み）
    std::vector<RobotAgent*> getNeighborsWithinDistance(
        const std::vector<RobotAgent*>& robots, double radius);
    DifferentialDrive computeDifferentialDrive(double linearVel, double angularVel);
    Velocity projectVelocityToOrientation(double vx, double vy);
    void applyMinimalShaking(double dt);
    Position findPathAroundHumanFeet(
        const std::vector<HumanSpot>& humanSpots,
        const std::vector<RobotAgent*>& robots);
};

}  // namespace swarm_control
