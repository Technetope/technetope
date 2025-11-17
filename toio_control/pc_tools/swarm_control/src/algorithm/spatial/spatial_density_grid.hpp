#pragma once

#include <vector>
#include <optional>
#include <string>
#include <limits>
#include "../config/params.hpp"

namespace swarm_control {

// 前方宣言
class RobotAgent;

struct DirectionalDensity {
    double angle = 0.0;         // 方向 (rad)
    double density = 0.0;      // 混雑度
    double x = 0.0, y = 0.0;   // サンプル位置
};

struct GridLayer {
    double cellSize = 50.0;     // セルサイズ (mm)
    double weight = 1.0;        // 重み
    std::string name;          // レイヤー名
    std::vector<std::vector<int>> grid;  // グリッド（各セルの混雑度）
    int gridWidth = 0;         // グリッド幅（セル数）
    int gridHeight = 0;        // グリッド高さ（セル数）
};

class SpatialDensityGrid {
public:
    SpatialDensityGrid();
    
    // グリッドを更新（ロボット位置から）
    void update(const std::vector<RobotAgent*>& robots);
    
    // 位置の混雑度を取得（3レイヤーを重み付けして合成）
    double getDensity(double x, double y) const;
    
    // 8方向の混雑度を取得
    std::vector<DirectionalDensity> getDirectionalDensities(
        double x, double y, double radius = 100.0) const;
    
    // 最も混雑度が低い方向を取得
    std::optional<DirectionalDensity> getLeastCrowdedDirection(
        double x, double y, double radius = 100.0) const;

private:
    // 3レイヤーのグリッド
    std::vector<GridLayer> layers_;
    
    // フィールドサイズ
    double fieldMinX_ = 0.0;
    double fieldMinY_ = 0.0;
    double fieldMaxX_ = 0.0;
    double fieldMaxY_ = 0.0;
    double fieldWidth_ = 0.0;
    double fieldHeight_ = 0.0;
    
    // 位置からグリッドセル座標を取得
    std::pair<int, int> getGridCell(double x, double y, int layerIdx) const;
    
    // グリッドを初期化
    void initialize();
    
    // グリッドをリセット
    void reset();
};

}  // namespace swarm_control

