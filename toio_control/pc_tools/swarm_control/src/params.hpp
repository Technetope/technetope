#pragma once

#include <vector>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace swarm_control {
namespace params {

struct FieldParams {
    double minX = 34.0;      // mm
    double minY = 35.0;      // mm
    double maxX = 949.0;     // mm
    double maxY = 898.0;     // mm
    double safetyMargin = 50.0;  // mm
};

struct PotentialParams {
    double lambda = 1.0;
    double beta = 2.5;
    double alpha = 0.9;
    double spotRadius = 200.0;  // mm
    double gridSize = 50.0;     // mm
    int updateInterval = 5;     // フレーム数
    double staticThreshold = 20.0;  // mm/s
    double headToFootDistance = 900.0;  // mm
    double footSeparation = 250.0;     // mm
    double footRadius = 100.0;         // mm
    double footStrength = 1.5;
};

struct UrgentParams {
    double threshold = 80.0;        // mm
    double escapeSpeed = 300.0;      // mm/s
    double duration = 2000.0;       // ms
    double threatSpeedThreshold = 50.0;  // mm/s
};

struct AutonomyParams {
    double minSpeed = 120.0;         // mm/s
    double maxSpeed = 250.0;         // mm/s
    double persistenceAngle = 0.25;  // rad
    double persistenceTime = 5.0;    // s
    double sharpTurnProbability = 0.03;
    double sharpTurnAngle = M_PI / 2;  // rad
    double preferredDistanceMin = 400.0;  // mm
    double preferredDistanceMax = 700.0;  // mm
    double nearAreaProbability = 0.2;
    double randomWalkInterval = 7.0;  // s
    double speedChangeInterval = 2.0;  // s
    int speedLevels = 3;
    double homeostasisTarget = 0.7;
    double homeostasisDecay = 0.01;
    double homeostasisThreshold = 0.3;
    double wanderingProbability = 0.01;
    double wanderingDuration = 3.0;  // s
    double orientationAlignment = 0.7;
    double maxAngularSpeed = M_PI / 3;  // rad/s
    double rotationStopDuration = 0.1;  // s
    int positionHistorySize = 30;
    double stationaryThreshold = 30.0;  // mm/s
    double stationaryTimeThreshold = 1.0;  // s
    
    struct SpeedDistribution {
        double fast = 0.2;      // 20%
        double moderate = 0.6;  // 60%
        double slow = 0.2;      // 20%
    } speedDistribution;
};

struct CollisionParams {
    double safeDistance = 80.0;      // mm
    double stopDistance = 75.0;     // mm
    double repulsionGain = 6000.0;
    double boundaryGain = 3200.0;
    double minScale = 0.05;
    double minSeparation = 76.0;     // mm
    double separationForce = 3000.0;
    double emergencyStopDistance = 71.0;  // mm
    double emergencyStopSpeed = 0.15;
};

struct DensityGridParams {
    double sampleRadius = 100.0;    // mm
    double repulsionGain = 2000.0;
    double weightCrowdAvoidance = 0.4;
    double weightPredictive = 0.3;
    double weightCollision = 0.3;
};

struct PredictiveParams {
    std::vector<double> predictionTimes = {0.5, 1.0, 2.0};  // s
    double predictionRadius = 76.0;  // mm
    double repulsionGain = 1500.0;
    double uncertaintyGrowth = 0.2;
};

struct ClusteringParams {
    double clusterDistance = 100.0;  // mm
    double contourSampleRadius = 150.0;  // mm
    double escapeForce = 3000.0;
};

struct BoidParams {
    double separationDistance = 80.0;   // mm
    double alignmentDistance = 150.0;    // mm
    double cohesionDistance = 200.0;     // mm
    double separationWeight = 0.5;       // 分離の重み
    double alignmentWeight = 0.3;         // 整列の重み
    double cohesionWeight = 0.2;         // 結合の重み
    double interactionWeight = 0.4;       // 相互作用の重み（発動時）
    double autonomyWeight = 0.6;          // 自律性の重み
    double activationProbability = 0.15;  // 群れ行動発動確率（15%）
};

struct RobotParams {
    int count = 30;
    double radius = 35.5;  // mm (toioの最長辺/2)
    double maxSpeed = 250.0;  // mm/s
    double maxAngularSpeed = M_PI / 3;  // rad/s
};

struct AllParams {
    FieldParams field;
    PotentialParams potential;
    UrgentParams urgent;
    AutonomyParams autonomy;
    CollisionParams collision;
    DensityGridParams densityGrid;
    PredictiveParams predictive;
    ClusteringParams clustering;
    BoidParams boid;
    RobotParams robot;
};

// グローバルパラメータインスタンス
extern AllParams g_params;

// パラメータを初期化
void initializeParams();

}  // namespace params
}  // namespace swarm_control

