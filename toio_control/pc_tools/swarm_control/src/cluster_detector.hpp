#pragma once

#include <vector>
#include <optional>
#include <limits>
#include "params.hpp"
#include "position.hpp"

namespace swarm_control {

// 前方宣言
class RobotAgent;

struct Cluster {
    std::vector<int> indices;              // クラスタ内のロボットインデックス
    std::vector<Position> positions;      // ロボット位置
    double centerX = 0.0;                // クラスタの中心
    double centerY = 0.0;
};

struct Contour {
    Cluster cluster;                      // クラスタ情報
    std::vector<Position> hull;          // 凸包（輪郭）
    double centerX = 0.0;                 // 輪郭の中心
    double centerY = 0.0;
};

struct EscapeDirection {
    double x = 0.0;      // 方向ベクトル X
    double y = 0.0;      // 方向ベクトル Y
    double strength = 0.0;  // 強度
};

class ClusterDetector {
public:
    ClusterDetector();
    
    // 距離ベースクラスタリング（100mm以内のロボットをグループ化）
    std::vector<Cluster> detectClusters(
        const std::vector<RobotAgent*>& robots);
    
    // 輪郭（凸包）を推定
    std::vector<Contour> estimateContours(
        const std::vector<RobotAgent*>& robots);
    
    // ロボットがクラスタ内にいるかチェック
    bool isInCluster(const RobotAgent* robot, const Cluster& cluster) const;
    
    // ロボットから輪郭の外側方向を計算
    std::optional<EscapeDirection> getEscapeDirection(
        const RobotAgent* robot, const Contour& contour) const;

private:
    // 凸包（convex hull）を計算（Graham scan風の簡易版）
    std::vector<Position> computeConvexHull(
        const std::vector<Position>& points) const;
    
    // 外積を計算（右回り判定用）
    double crossProduct(const Position& p1, const Position& p2, 
                       const Position& p3) const;
    
    std::vector<Cluster> clusters_;
    std::vector<Contour> contours_;
};

}  // namespace swarm_control
