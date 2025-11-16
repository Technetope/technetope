#include "spatial_density_grid.hpp"
#include "robot_agent.hpp"
#include <algorithm>
#include <cmath>

namespace swarm_control {

SpatialDensityGrid::SpatialDensityGrid() {
    // フィールドサイズを取得
    fieldMinX_ = params::g_params.field.minX;
    fieldMinY_ = params::g_params.field.minY;
    fieldMaxX_ = params::g_params.field.maxX;
    fieldMaxY_ = params::g_params.field.maxY;
    fieldWidth_ = fieldMaxX_ - fieldMinX_;
    fieldHeight_ = fieldMaxY_ - fieldMinY_;
    
    // 3レイヤーを初期化
    layers_.resize(3);
    layers_[0] = {50.0, 1.0, "near"};
    layers_[1] = {100.0, 0.6, "medium"};
    layers_[2] = {200.0, 0.3, "far"};
    
    initialize();
}

void SpatialDensityGrid::initialize() {
    for (auto& layer : layers_) {
        const double cellSize = layer.cellSize;
        layer.gridWidth = static_cast<int>(std::ceil(fieldWidth_ / cellSize)) + 1;
        layer.gridHeight = static_cast<int>(std::ceil(fieldHeight_ / cellSize)) + 1;
        
        // グリッドを初期化
        layer.grid.clear();
        layer.grid.resize(layer.gridHeight);
        for (int y = 0; y < layer.gridHeight; ++y) {
            layer.grid[y].resize(layer.gridWidth, 0);
        }
    }
}

void SpatialDensityGrid::reset() {
    for (auto& layer : layers_) {
        for (auto& row : layer.grid) {
            std::fill(row.begin(), row.end(), 0);
        }
    }
}

void SpatialDensityGrid::update(const std::vector<RobotAgent*>& robots) {
    reset();
    
    // 各ロボットの位置をグリッドに追加
    for (const auto* robot : robots) {
        auto pos = robot->getPosition();
        
        for (size_t layerIdx = 0; layerIdx < layers_.size(); ++layerIdx) {
            const double cellSize = layers_[layerIdx].cellSize;
            const int gridX = static_cast<int>(std::floor((pos.x - fieldMinX_) / cellSize));
            const int gridY = static_cast<int>(std::floor((pos.y - fieldMinY_) / cellSize));
            
            // グリッド範囲内かチェック
            if (gridX >= 0 && gridX < layers_[layerIdx].gridWidth &&
                gridY >= 0 && gridY < layers_[layerIdx].gridHeight) {
                layers_[layerIdx].grid[gridY][gridX]++;
            }
        }
    }
}

std::pair<int, int> SpatialDensityGrid::getGridCell(double x, double y, int layerIdx) const {
    const double cellSize = layers_[layerIdx].cellSize;
    const int gridX = static_cast<int>(std::floor((x - fieldMinX_) / cellSize));
    const int gridY = static_cast<int>(std::floor((y - fieldMinY_) / cellSize));
    return {gridX, gridY};
}

double SpatialDensityGrid::getDensity(double x, double y) const {
    double totalDensity = 0.0;
    double totalWeight = 0.0;
    
    for (size_t layerIdx = 0; layerIdx < layers_.size(); ++layerIdx) {
        const auto [gridX, gridY] = getGridCell(x, y, layerIdx);
        const auto& layer = layers_[layerIdx];
        
        // グリッド範囲内かチェック
        if (gridX >= 0 && gridX < layer.gridWidth &&
            gridY >= 0 && gridY < layer.gridHeight) {
            const int density = layer.grid[gridY][gridX];
            totalDensity += density * layer.weight;
            totalWeight += layer.weight;
        }
    }
    
    // 重みで正規化
    return totalWeight > 0.0 ? totalDensity / totalWeight : 0.0;
}

std::vector<DirectionalDensity> SpatialDensityGrid::getDirectionalDensities(
    double x, double y, double radius) const {
    
    const std::vector<std::pair<double, std::pair<double, double>>> directions = {
        {0.0, {1.0, 0.0}},                    // 右
        {M_PI / 4, {1.0, 1.0}},              // 右上
        {M_PI / 2, {0.0, 1.0}},              // 上
        {3.0 * M_PI / 4, {-1.0, 1.0}},       // 左上
        {M_PI, {-1.0, 0.0}},                 // 左
        {5.0 * M_PI / 4, {-1.0, -1.0}},      // 左下
        {3.0 * M_PI / 2, {0.0, -1.0}},       // 下
        {7.0 * M_PI / 4, {1.0, -1.0}}        // 右下
    };
    
    std::vector<DirectionalDensity> densities;
    densities.reserve(8);
    
    for (const auto& [angle, dir] : directions) {
        const double sampleX = x + dir.first * radius;
        const double sampleY = y + dir.second * radius;
        const double density = getDensity(sampleX, sampleY);
        
        DirectionalDensity dd;
        dd.angle = angle;
        dd.density = density;
        dd.x = sampleX;
        dd.y = sampleY;
        densities.push_back(dd);
    }
    
    return densities;
}

std::optional<DirectionalDensity> SpatialDensityGrid::getLeastCrowdedDirection(
    double x, double y, double radius) const {
    
    const auto densities = getDirectionalDensities(x, y, radius);
    
    // 混雑度が最も低い方向を探す
    double minDensity = std::numeric_limits<double>::max();
    std::optional<DirectionalDensity> bestDirection;
    
    for (const auto& dir : densities) {
        if (dir.density < minDensity) {
            minDensity = dir.density;
            bestDirection = dir;
        }
    }
    
    return bestDirection;
}

}  // namespace swarm_control

