#pragma once

#include <vector>
#include "params.hpp"
#include "human_spot.hpp"

namespace swarm_control {

// 前方宣言
class RobotAgent;

struct Gradient {
    double gx = 0.0;  // 勾配ベクトル X
    double gy = 0.0;  // 勾配ベクトル Y
};

class SpotPotentialField {
public:
    SpotPotentialField();
    
    // 初期化
    void initialize();
    
    // スポットを更新（ポテンシャル場を再計算）
    void updateSpots(const std::vector<HumanSpot>& spots,
                     const std::vector<RobotAgent*>& robots);
    
    // 位置の勾配を取得
    Gradient getGradient(double x, double y) const;
    
    // 位置のポテンシャル値を取得
    double getPotential(double x, double y) const;

private:
    // スポットからポテンシャルを追加
    void addSpotPotential(const HumanSpot& spot);
    
    // 人間の足のポテンシャルを追加
    void addFootPotential(const HumanSpot& spot);
    
    // 単一の足のポテンシャルを追加
    void addSingleFootPotential(double footX, double footY, 
                               double radius, double strength);
    
    // グリッドベースのポテンシャル場
    std::vector<std::vector<double>> grid_;  // ポテンシャル値
    int gridWidth_ = 0;                      // グリッド幅
    int gridHeight_ = 0;                     // グリッド高さ
    double cellSize_ = 50.0;                 // セルサイズ (mm)
    
    // フィールドサイズ
    double fieldMinX_ = 0.0;
    double fieldMinY_ = 0.0;
    double fieldMaxX_ = 0.0;
    double fieldMaxY_ = 0.0;
    double fieldWidth_ = 0.0;
    double fieldHeight_ = 0.0;
    
    // 更新カウンター
    int updateCounter_ = 0;
    
    // 位置からグリッドセル座標を取得
    std::pair<int, int> getGridCell(double x, double y) const;
    
    // グリッドセル座標から位置を取得
    std::pair<double, double> getPosition(int gridX, int gridY) const;
    
    std::vector<HumanSpot> spots_;
};

}  // namespace swarm_control

