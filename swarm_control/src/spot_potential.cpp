#include "spot_potential.hpp"
#include "robot_agent.hpp"
#include "utils.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace swarm_control {

SpotPotentialField::SpotPotentialField() {
    cellSize_ = params::g_params.potential.gridSize;
    fieldMinX_ = params::g_params.field.minX;
    fieldMinY_ = params::g_params.field.minY;
    fieldMaxX_ = params::g_params.field.maxX;
    fieldMaxY_ = params::g_params.field.maxY;
    fieldWidth_ = fieldMaxX_ - fieldMinX_;
    fieldHeight_ = fieldMaxY_ - fieldMinY_;
    initialize();
}

void SpotPotentialField::initialize() {
    gridWidth_ = static_cast<int>(std::ceil(fieldWidth_ / cellSize_)) + 1;
    gridHeight_ = static_cast<int>(std::ceil(fieldHeight_ / cellSize_)) + 1;
    
    grid_.clear();
    grid_.resize(gridHeight_);
    for (int y = 0; y < gridHeight_; ++y) {
        grid_[y].resize(gridWidth_, 0.0);
    }
}

std::pair<int, int> SpotPotentialField::getGridCell(double x, double y) const {
    const int gridX = static_cast<int>(std::floor((x - fieldMinX_) / cellSize_));
    const int gridY = static_cast<int>(std::floor((y - fieldMinY_) / cellSize_));
    return {gridX, gridY};
}

std::pair<double, double> SpotPotentialField::getPosition(int gridX, int gridY) const {
    const double x = fieldMinX_ + (gridX + 0.5) * cellSize_;
    const double y = fieldMinY_ + (gridY + 0.5) * cellSize_;
    return {x, y};
}

void SpotPotentialField::updateSpots(const std::vector<HumanSpot>& spots,
                                      const std::vector<RobotAgent*>& robots) {
    spots_ = spots;
    updateCounter_++;
    
    // 更新間隔をチェック
    if (updateCounter_ % params::g_params.potential.updateInterval != 0) {
        return;
    }
    
    // グリッドをリセット
    for (auto& row : grid_) {
        std::fill(row.begin(), row.end(), 0.0);
    }
    
    // 各スポットからポテンシャルを計算
    for (const auto& spot : spots) {
        addSpotPotential(spot);
        addFootPotential(spot);
    }
}

void SpotPotentialField::addSpotPotential(const HumanSpot& spot) {
    const double spotRadius = params::g_params.potential.spotRadius;
    const double lambda = params::g_params.potential.lambda;
    const double beta = params::g_params.potential.beta;
    
    // グリッド範囲を計算
    const int minGridX = std::max(0, static_cast<int>(std::floor((spot.headX - spotRadius - fieldMinX_) / cellSize_)));
    const int maxGridX = std::min(gridWidth_ - 1, static_cast<int>(std::ceil((spot.headX + spotRadius - fieldMinX_) / cellSize_)));
    const int minGridY = std::max(0, static_cast<int>(std::floor((spot.headY - spotRadius - fieldMinY_) / cellSize_)));
    const int maxGridY = std::min(gridHeight_ - 1, static_cast<int>(std::ceil((spot.headY + spotRadius - fieldMinY_) / cellSize_)));
    
    // グリッドセルにポテンシャルを加算
    for (int gy = minGridY; gy <= maxGridY; ++gy) {
        for (int gx = minGridX; gx <= maxGridX; ++gx) {
            const auto [worldX, worldY] = getPosition(gx, gy);
            const double dist = utils::distance(worldX, worldY, spot.headX, spot.headY);
            
            if (dist < spotRadius) {
                // 負のポテンシャル（反発）
                const double potential = -lambda * std::exp(-beta * dist / spotRadius);
                grid_[gy][gx] += potential;
            }
        }
    }
}

void SpotPotentialField::addFootPotential(const HumanSpot& spot) {
    const double headToFoot = params::g_params.potential.headToFootDistance;
    const double footSep = params::g_params.potential.footSeparation;
    const double footRadius = params::g_params.potential.footRadius;
    const double footStrength = params::g_params.potential.footStrength;
    
    // 頭の進行方向を考慮して足の位置を計算
    const double speed = utils::distance(0.0, 0.0, spot.velocityX, spot.velocityY);
    double heading;
    if (speed > 1.0) {
        heading = std::atan2(spot.velocityY, spot.velocityX);
    } else {
        heading = M_PI / 2.0; // 下方向（デフォルト）
    }
    const double perpHeading = heading + M_PI / 2.0;
    
    // 左右の足の位置
    const double leftFootX = spot.headX + std::cos(perpHeading) * (footSep / 2.0) + std::cos(heading) * headToFoot;
    const double leftFootY = spot.headY + std::sin(perpHeading) * (footSep / 2.0) + std::sin(heading) * headToFoot;
    const double rightFootX = spot.headX - std::cos(perpHeading) * (footSep / 2.0) + std::cos(heading) * headToFoot;
    const double rightFootY = spot.headY - std::sin(perpHeading) * (footSep / 2.0) + std::sin(heading) * headToFoot;
    
    // 左右の足それぞれに負のポテンシャルを追加
    addSingleFootPotential(leftFootX, leftFootY, footRadius, footStrength);
    addSingleFootPotential(rightFootX, rightFootY, footRadius, footStrength);
}

void SpotPotentialField::addSingleFootPotential(double footX, double footY, 
                                                double radius, double strength) {
    const double lambda = params::g_params.potential.lambda;
    const double beta = params::g_params.potential.beta;
    
    // グリッド範囲を計算
    const int minGridX = std::max(0, static_cast<int>(std::floor((footX - radius - fieldMinX_) / cellSize_)));
    const int maxGridX = std::min(gridWidth_ - 1, static_cast<int>(std::ceil((footX + radius - fieldMinX_) / cellSize_)));
    const int minGridY = std::max(0, static_cast<int>(std::floor((footY - radius - fieldMinY_) / cellSize_)));
    const int maxGridY = std::min(gridHeight_ - 1, static_cast<int>(std::ceil((footY + radius - fieldMinY_) / cellSize_)));
    
    // グリッドセルにポテンシャルを加算
    for (int gy = minGridY; gy <= maxGridY; ++gy) {
        for (int gx = minGridX; gx <= maxGridX; ++gx) {
            const auto [worldX, worldY] = getPosition(gx, gy);
            const double dist = utils::distance(worldX, worldY, footX, footY);
            
            if (dist < radius) {
                // 負のポテンシャル（反発）
                const double potential = -lambda * strength * std::exp(-beta * dist / radius);
                grid_[gy][gx] += potential;
            }
        }
    }
}

Gradient SpotPotentialField::getGradient(double x, double y) const {
    const auto [gridX, gridY] = getGridCell(x, y);
    
    Gradient grad;
    
    // 境界チェック
    if (gridX < 1 || gridX >= gridWidth_ - 1 || gridY < 1 || gridY >= gridHeight_ - 1) {
        return grad;
    }
    
    // 有限差分法で勾配を計算
    grad.gx = (grid_[gridY][gridX + 1] - grid_[gridY][gridX - 1]) / (2.0 * cellSize_);
    grad.gy = (grid_[gridY + 1][gridX] - grid_[gridY - 1][gridX]) / (2.0 * cellSize_);
    
    return grad;
}

double SpotPotentialField::getPotential(double x, double y) const {
    const auto [gridX, gridY] = getGridCell(x, y);
    
    if (gridX < 0 || gridX >= gridWidth_ || gridY < 0 || gridY >= gridHeight_) {
        return 0.0;
    }
    
    return grid_[gridY][gridX];
}

}  // namespace swarm_control

