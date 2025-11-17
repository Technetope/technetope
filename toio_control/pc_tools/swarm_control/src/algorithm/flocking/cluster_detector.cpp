#include "cluster_detector.hpp"
#include "../agent/robot_agent.hpp"
#include "../../utils/utils.hpp"
#include <algorithm>
#include <cmath>
#include <set>

namespace swarm_control {

ClusterDetector::ClusterDetector() = default;

std::vector<Cluster> ClusterDetector::detectClusters(
    const std::vector<RobotAgent*>& robots) {
    
    clusters_.clear();
    std::set<int> processed;
    const double clusterDistance = params::g_params.clustering.clusterDistance;
    
    for (size_t i = 0; i < robots.size(); ++i) {
        if (processed.count(static_cast<int>(i)) > 0) continue;
        
        // 新しいクラスタを開始
        Cluster cluster;
        cluster.indices.push_back(static_cast<int>(i));
        auto pos = robots[i]->getPosition();
        cluster.positions.push_back({pos.x, pos.y});
        cluster.centerX = pos.x;
        cluster.centerY = pos.y;
        
        processed.insert(static_cast<int>(i));
        
        // 近接ロボットを収集
        bool changed = true;
        while (changed) {
            changed = false;
            for (size_t j = 0; j < robots.size(); ++j) {
                if (processed.count(static_cast<int>(j)) > 0) continue;
                
                // クラスタ内のいずれかのロボットと近接しているかチェック
                for (int idx : cluster.indices) {
                    auto pos1 = robots[idx]->getPosition();
                    auto pos2 = robots[j]->getPosition();
                    const double dist = utils::distance(pos1.x, pos1.y, pos2.x, pos2.y);
                    
                    if (dist < clusterDistance) {
                        cluster.indices.push_back(static_cast<int>(j));
                        cluster.positions.push_back({pos2.x, pos2.y});
                        processed.insert(static_cast<int>(j));
                        changed = true;
                        break;
                    }
                }
            }
        }
        
        // クラスタの中心を計算
        if (!cluster.positions.empty()) {
            double sumX = 0.0, sumY = 0.0;
            for (const auto& pos : cluster.positions) {
                sumX += pos.x;
                sumY += pos.y;
            }
            cluster.centerX = sumX / cluster.positions.size();
            cluster.centerY = sumY / cluster.positions.size();
        }
        
        // クラスタが2つ以上のロボットを含む場合のみ追加
        if (cluster.indices.size() >= 2) {
            clusters_.push_back(cluster);
        }
    }
    
    return clusters_;
}

double ClusterDetector::crossProduct(const Position& p1, const Position& p2, 
                                     const Position& p3) const {
    return (p2.x - p1.x) * (p3.y - p1.y) - (p2.y - p1.y) * (p3.x - p1.x);
}

std::vector<Position> ClusterDetector::computeConvexHull(
    const std::vector<Position>& points) const {
    
    if (points.size() < 3) {
        return points;
    }
    
    // 最も下（yが最大、同じならxが最小）の点を見つける
    size_t bottomIdx = 0;
    for (size_t i = 1; i < points.size(); ++i) {
        if (points[i].y > points[bottomIdx].y ||
            (std::abs(points[i].y - points[bottomIdx].y) < 0.001 && 
             points[i].x < points[bottomIdx].x)) {
            bottomIdx = i;
        }
    }
    
    // 最も下の点を最初に移動
    const Position bottomPoint = points[bottomIdx];
    std::vector<Position> sortedPoints = {bottomPoint};
    
    // 残りの点を角度でソート
    std::vector<Position> otherPoints;
    for (size_t i = 0; i < points.size(); ++i) {
        if (i != bottomIdx) {
            otherPoints.push_back(points[i]);
        }
    }
    
    std::sort(otherPoints.begin(), otherPoints.end(),
        [&bottomPoint](const Position& a, const Position& b) {
            const double angleA = std::atan2(a.y - bottomPoint.y, a.x - bottomPoint.x);
            const double angleB = std::atan2(b.y - bottomPoint.y, b.x - bottomPoint.x);
            if (std::abs(angleA - angleB) < 0.001) {
                const double distA = utils::distance(a.x, a.y, bottomPoint.x, bottomPoint.y);
                const double distB = utils::distance(b.x, b.y, bottomPoint.x, bottomPoint.y);
                return distA < distB;
            }
            return angleA < angleB;
        });
    
    sortedPoints.insert(sortedPoints.end(), otherPoints.begin(), otherPoints.end());
    
    // Graham scan風のアルゴリズム
    std::vector<Position> hull = {sortedPoints[0], sortedPoints[1]};
    
    for (size_t i = 2; i < sortedPoints.size(); ++i) {
        const Position& point = sortedPoints[i];
        
        // 右回り（時計回り）の場合は前の点を削除
        while (hull.size() >= 2) {
            const Position& p1 = hull[hull.size() - 2];
            const Position& p2 = hull[hull.size() - 1];
            
            const double cross = crossProduct(p1, p2, point);
            if (cross <= 0.0) {
                hull.pop_back();
            } else {
                break;
            }
        }
        
        hull.push_back(point);
    }
    
    return hull;
}

std::vector<Contour> ClusterDetector::estimateContours(
    const std::vector<RobotAgent*>& robots) {
    
    contours_.clear();
    
    // クラスタを検出
    const auto clusters = detectClusters(robots);
    
    // 各クラスタの凸包を計算
    for (const auto& cluster : clusters) {
        const auto hull = computeConvexHull(cluster.positions);
        Contour contour;
        contour.cluster = cluster;
        contour.hull = hull;
        contour.centerX = cluster.centerX;
        contour.centerY = cluster.centerY;
        contours_.push_back(contour);
    }
    
    return contours_;
}

bool ClusterDetector::isInCluster(const RobotAgent* robot, const Cluster& cluster) const {
    const int robotIndex = robot->getIndex();
    return std::find(cluster.indices.begin(), cluster.indices.end(), robotIndex) 
           != cluster.indices.end();
}

std::optional<EscapeDirection> ClusterDetector::getEscapeDirection(
    const RobotAgent* robot, const Contour& contour) const {
    
    // ロボットがクラスタ内にいるかチェック
    if (!isInCluster(robot, contour.cluster)) {
        return std::nullopt;
    }
    
    auto robotPos = robot->getPosition();
    
    // 輪郭（凸包）の各辺から、ロボットへの方向を計算
    EscapeDirection bestDirection;
    double maxDistance = -1.0;
    
    for (size_t i = 0; i < contour.hull.size(); ++i) {
        const Position& p1 = contour.hull[i];
        const Position& p2 = contour.hull[(i + 1) % contour.hull.size()];
        
        // 輪郭の辺の中点
        const double midX = (p1.x + p2.x) / 2.0;
        const double midY = (p1.y + p2.y) / 2.0;
        
        // ロボットから輪郭の中点への方向
        const double dx = midX - robotPos.x;
        const double dy = midY - robotPos.y;
        const double dist = utils::distance(robotPos.x, robotPos.y, midX, midY);
        
        if (dist > maxDistance) {
            maxDistance = dist;
            // 外側方向（輪郭から離れる方向）を計算
            const double angle = std::atan2(dy, dx);
            const double normalAngle = angle + M_PI; // 180度回転
            
            bestDirection.x = std::cos(normalAngle);
            bestDirection.y = std::sin(normalAngle);
            bestDirection.strength = params::g_params.clustering.escapeForce * 
                                     (1.0 + dist / 100.0);
        }
    }
    
    return bestDirection;
}

}  // namespace swarm_control

