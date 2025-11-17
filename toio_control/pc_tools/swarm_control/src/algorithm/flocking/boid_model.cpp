#include "boid_model.hpp"
#include "../agent/robot_agent.hpp"
#include "../../utils/utils.hpp"
#include <cmath>
#include <random>
#include <map>
#include <string>

namespace swarm_control {

BoidModel::BoidModel() {
    cellSize_ = 200.0;
}

BoidModel::SpatialHash BoidModel::getHashKey(double x, double y) const {
    SpatialHash hash;
    hash.cellX = static_cast<int>(std::floor(x / cellSize_));
    hash.cellY = static_cast<int>(std::floor(y / cellSize_));
    return hash;
}

void BoidModel::updateSpatialHash(const std::vector<RobotAgent*>& robots) {
    spatialHash_.clear();
    
    for (size_t i = 0; i < robots.size(); ++i) {
        auto pos = robots[i]->getPosition();
        const SpatialHash key = getHashKey(pos.x, pos.y);
        
        if (spatialHash_.find(key) == spatialHash_.end()) {
            spatialHash_[key] = std::vector<int>();
        }
        spatialHash_[key].push_back(static_cast<int>(i));
    }
}

std::vector<int> BoidModel::getNeighbors(
    const RobotAgent* robot,
    const std::vector<RobotAgent*>& robots) const {
    
    std::vector<int> neighbors;
    auto pos = robot->getPosition();
    const SpatialHash centerKey = getHashKey(pos.x, pos.y);
    const int robotIndex = robot->getIndex();  // 自分自身のインデックスを取得
    
    // 現在のセルと周囲8セルをチェック
    for (int dx = -1; dx <= 1; ++dx) {
        for (int dy = -1; dy <= 1; ++dy) {
            SpatialHash key;
            key.cellX = centerKey.cellX + dx;
            key.cellY = centerKey.cellY + dy;
            
            auto it = spatialHash_.find(key);
            if (it != spatialHash_.end()) {
                for (int idx : it->second) {
                    // インデックス範囲チェックと自分自身の除外
                    if (idx != robotIndex && idx >= 0 && 
                        static_cast<size_t>(idx) < robots.size()) {
                        neighbors.push_back(idx);
                    }
                }
            }
        }
    }
    
    return neighbors;
}

Velocity BoidModel::computeSeparation(const RobotAgent* robot,
                                       const std::vector<int>& neighbors,
                                       const std::vector<RobotAgent*>& robots) const {
    Velocity sep;
    auto pos1 = robot->getPosition();
    const double separationDistance = params::g_params.boid.separationDistance;
    
    for (int j : neighbors) {
        // インデックス範囲チェック
        if (j < 0 || static_cast<size_t>(j) >= robots.size()) {
            continue;
        }
        
        auto pos2 = robots[j]->getPosition();
        const double dx = pos1.x - pos2.x;
        const double dy = pos1.y - pos2.y;
        const double dist = utils::distance(pos1.x, pos1.y, pos2.x, pos2.y);
        
        if (dist < separationDistance && dist > utils::EPSILON_DISTANCE) {
            const double strength = 1.0 / dist;
            sep.vx += dx * strength;
            sep.vy += dy * strength;
        }
    }
    
    return sep;
}

Velocity BoidModel::computeAlignment(const RobotAgent* robot,
                                      const std::vector<int>& neighbors,
                                      const std::vector<RobotAgent*>& robots,
                                      const std::vector<Velocity>& robotStates) const {
    Velocity align;
    auto pos1 = robot->getPosition();
    const double alignmentDistance = params::g_params.boid.alignmentDistance;
    int count = 0;
    
    for (int j : neighbors) {
        // インデックス範囲チェック
        if (j < 0 || static_cast<size_t>(j) >= robots.size() || 
            static_cast<size_t>(j) >= robotStates.size()) {
            continue;
        }
        
        auto pos2 = robots[j]->getPosition();
        const double dist = utils::distance(pos1.x, pos1.y, pos2.x, pos2.y);
        
        if (dist < alignmentDistance && dist > utils::EPSILON_DISTANCE) {
            align.vx += robotStates[j].vx;
            align.vy += robotStates[j].vy;
            count++;
        }
    }
    
    if (count > 0) {
        align.vx /= count;
        align.vy /= count;
        
        // 正規化して最大速度を適用
        const double alignSpeed = utils::distance(0.0, 0.0, align.vx, align.vy);
        if (alignSpeed > 0.0) {
            const double maxSpeed = params::g_params.robot.maxSpeed;
            align.vx = (align.vx / alignSpeed) * maxSpeed;
            align.vy = (align.vy / alignSpeed) * maxSpeed;
        }
    }
    
    return align;
}

Velocity BoidModel::computeCohesion(const RobotAgent* robot,
                                    const std::vector<int>& neighbors,
                                    const std::vector<RobotAgent*>& robots) const {
    Velocity coh;
    auto pos1 = robot->getPosition();
    const double cohesionDistance = params::g_params.boid.cohesionDistance;
    int count = 0;
    
    double centerX = 0.0, centerY = 0.0;
    for (int j : neighbors) {
        // インデックス範囲チェック
        if (j < 0 || static_cast<size_t>(j) >= robots.size()) {
            continue;
        }
        
        auto pos2 = robots[j]->getPosition();
        const double dist = utils::distance(pos1.x, pos1.y, pos2.x, pos2.y);
        
        if (dist < cohesionDistance && dist > utils::EPSILON_DISTANCE) {
            centerX += pos2.x;
            centerY += pos2.y;
            count++;
        }
    }
    
    if (count > 0) {
        centerX = (centerX / count) - pos1.x;
        centerY = (centerY / count) - pos1.y;
        
        // 正規化して最大速度を適用
        const double cohDist = utils::distance(0.0, 0.0, centerX, centerY);
        if (cohDist > 0.0) {
            const double maxSpeed = params::g_params.robot.maxSpeed;
            coh.vx = (centerX / cohDist) * maxSpeed;
            coh.vy = (centerY / cohDist) * maxSpeed;
        }
    }
    
    return coh;
}

void BoidModel::applyFlocking(std::vector<RobotAgent*>& robots,
                              std::vector<Velocity>& robotStates,
                              double dt) {
    (void)dt;  // 現状の実装では未使用
    updateSpatialHash(robots);
    
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    
    const double activationProbability = params::g_params.boid.activationProbability;
    const double separationWeight = params::g_params.boid.separationWeight;
    const double alignmentWeight = params::g_params.boid.alignmentWeight;
    const double cohesionWeight = params::g_params.boid.cohesionWeight;
    const double interactionWeight = params::g_params.boid.interactionWeight;
    const double autonomyWeight = params::g_params.boid.autonomyWeight;
    
    for (size_t i = 0; i < robots.size(); ++i) {
        // 確率的に群れ行動を発動
        if (dist(gen) >= activationProbability) {
            continue;
        }
        
        // 近傍ロボットを取得
        const auto neighbors = getNeighbors(robots[i], robots);
        
        if (!neighbors.empty()) {
            // 分離・整列・結合を計算
            const auto separation = computeSeparation(robots[i], neighbors, robots);
            const auto alignment = computeAlignment(robots[i], neighbors, robots, robotStates);
            const auto cohesion = computeCohesion(robots[i], neighbors, robots);
            
            // 自律性（既存の速度）
            const double autonomyX = robotStates[i].vx * autonomyWeight;
            const double autonomyY = robotStates[i].vy * autonomyWeight;
            
            // 重み付け合成
            const double flockX = separationWeight * separation.vx +
                                 alignmentWeight * alignment.vx +
                                 cohesionWeight * cohesion.vx;
            const double flockY = separationWeight * separation.vy +
                                 alignmentWeight * alignment.vy +
                                 cohesionWeight * cohesion.vy;
            
            // 相互作用と自律性のバランス
            robotStates[i].vx = interactionWeight * flockX + autonomyX;
            robotStates[i].vy = interactionWeight * flockY + autonomyY;
        }
    }
}

}  // namespace swarm_control
