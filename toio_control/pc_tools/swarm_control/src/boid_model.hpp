#pragma once

#include <map>
#include <vector>
#include <string>
#include "params.hpp"
#include "velocity.hpp"

namespace swarm_control {

// 前方宣言
class RobotAgent;

class BoidModel {
public:
    BoidModel();
    
    // 群れ行動を適用
    void applyFlocking(std::vector<RobotAgent*>& robots,
                      std::vector<Velocity>& robotStates,
                      double dt);

private:
    // 空間ハッシュ（近傍検索の最適化）
    struct SpatialHash {
        int cellX = 0;
        int cellY = 0;
        
        bool operator<(const SpatialHash& other) const {
            return cellX < other.cellX || 
                   (cellX == other.cellX && cellY < other.cellY);
        }
    };
    
    // 近傍ロボットを取得（空間ハッシュを使用）
    std::vector<int> getNeighbors(
        const RobotAgent* robot,
        const std::vector<RobotAgent*>& robots) const;
    
    // 空間ハッシュを更新
    void updateSpatialHash(const std::vector<RobotAgent*>& robots);
    
    // Separation（分離）
    Velocity computeSeparation(const RobotAgent* robot,
                              const std::vector<int>& neighbors,
                              const std::vector<RobotAgent*>& robots) const;
    
    // Alignment（整列）
    Velocity computeAlignment(const RobotAgent* robot,
                             const std::vector<int>& neighbors,
                             const std::vector<RobotAgent*>& robots,
                             const std::vector<Velocity>& robotStates) const;
    
    // Cohesion（結束）
    Velocity computeCohesion(const RobotAgent* robot,
                            const std::vector<int>& neighbors,
                            const std::vector<RobotAgent*>& robots) const;
    
    // 空間ハッシュマップ
    std::map<SpatialHash, std::vector<int>> spatialHash_;
    double cellSize_ = 200.0;  // 空間ハッシュのセルサイズ (mm)
    
    // ハッシュキーを取得
    SpatialHash getHashKey(double x, double y) const;
};

}  // namespace swarm_control
