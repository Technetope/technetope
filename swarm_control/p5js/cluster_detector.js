// クラスタリングと輪郭検出（デッドロック解放）

class ClusterDetector {
    constructor() {
        this.clusters = [];
        this.contours = []; // 各クラスタの輪郭（凸包）
    }
    
    // 距離ベースクラスタリング（100mm以内のロボットをグループ化）
    detectClusters(robots) {
        this.clusters = [];
        const processed = new Set();
        const clusterDistance = Params.clustering.clusterDistance;
        
        for (let i = 0; i < robots.length; i++) {
            if (processed.has(i)) continue;
            
            // 新しいクラスタを開始
            const cluster = {
                indices: [i],
                positions: [{ x: robots[i].x, y: robots[i].y }],
                centerX: robots[i].x,
                centerY: robots[i].y
            };
            
            processed.add(i);
            
            // 近接ロボットを収集
            let changed = true;
            while (changed) {
                changed = false;
                for (let j = 0; j < robots.length; j++) {
                    if (processed.has(j)) continue;
                    
                    // クラスタ内のいずれかのロボットと近接しているかチェック
                    for (const idx of cluster.indices) {
                        const dx = robots[j].x - robots[idx].x;
                        const dy = robots[j].y - robots[idx].y;
                        const dist = Math.hypot(dx, dy);
                        
                        if (dist < clusterDistance) {
                            cluster.indices.push(j);
                            cluster.positions.push({ x: robots[j].x, y: robots[j].y });
                            processed.add(j);
                            changed = true;
                            break;
                        }
                    }
                }
            }
            
            // クラスタの中心を計算
            if (cluster.positions.length > 0) {
                let sumX = 0, sumY = 0;
                for (const pos of cluster.positions) {
                    sumX += pos.x;
                    sumY += pos.y;
                }
                cluster.centerX = sumX / cluster.positions.length;
                cluster.centerY = sumY / cluster.positions.length;
            }
            
            // クラスタが2つ以上のロボットを含む場合のみ追加（デッドロックの可能性）
            if (cluster.indices.length >= 2) {
                this.clusters.push(cluster);
            }
        }
        
        return this.clusters;
    }
    
    // 凸包（convex hull）を計算（Graham scan風の簡易版）
    computeConvexHull(points) {
        if (points.length < 3) {
            // 点が3つ未満の場合はそのまま返す
            return points;
        }
        
        // 最も下（yが最大、同じならxが最小）の点を見つける
        let bottomIdx = 0;
        for (let i = 1; i < points.length; i++) {
            if (points[i].y > points[bottomIdx].y ||
                (points[i].y === points[bottomIdx].y && points[i].x < points[bottomIdx].x)) {
                bottomIdx = i;
            }
        }
        
        // 最も下の点を最初に移動
        const bottomPoint = points[bottomIdx];
        const sortedPoints = [bottomPoint];
        
        // 残りの点を角度でソート（簡易版：距離と角度で判定）
        const otherPoints = points.filter((_, idx) => idx !== bottomIdx);
        otherPoints.sort((a, b) => {
            const angleA = Math.atan2(a.y - bottomPoint.y, a.x - bottomPoint.x);
            const angleB = Math.atan2(b.y - bottomPoint.y, b.x - bottomPoint.x);
            if (Math.abs(angleA - angleB) < 0.001) {
                // 角度が同じ場合は距離で比較
                const distA = Math.hypot(a.x - bottomPoint.x, a.y - bottomPoint.y);
                const distB = Math.hypot(b.x - bottomPoint.x, b.y - bottomPoint.y);
                return distA - distB;
            }
            return angleA - angleB;
        });
        
        sortedPoints.push(...otherPoints);
        
        // Graham scan風のアルゴリズム（簡易版）
        const hull = [sortedPoints[0], sortedPoints[1]];
        
        for (let i = 2; i < sortedPoints.length; i++) {
            const point = sortedPoints[i];
            
            // 右回り（時計回り）の場合は前の点を削除
            while (hull.length >= 2) {
                const p1 = hull[hull.length - 2];
                const p2 = hull[hull.length - 1];
                
                // 外積で右回りか判定
                const cross = (p2.x - p1.x) * (point.y - p1.y) - (p2.y - p1.y) * (point.x - p1.x);
                if (cross <= 0) {
                    hull.pop();
                } else {
                    break;
                }
            }
            
            hull.push(point);
        }
        
        return hull;
    }
    
    // 輪郭（contour）を推定（各クラスタの凸包を計算）
    estimateContours(robots) {
        this.contours = [];
        
        // クラスタを検出
        const clusters = this.detectClusters(robots);
        
        // 各クラスタの凸包を計算
        for (const cluster of clusters) {
            const hull = this.computeConvexHull(cluster.positions);
            this.contours.push({
                cluster: cluster,
                hull: hull,
                centerX: cluster.centerX,
                centerY: cluster.centerY
            });
        }
        
        return this.contours;
    }
    
    // ロボットがクラスタ内にいるかチェック
    isInCluster(robot, cluster) {
        for (const idx of cluster.indices) {
            if (robot.index === idx) {
                return true;
            }
        }
        return false;
    }
    
    // ロボットから輪郭の外側方向を計算
    getEscapeDirection(robot, contour) {
        // ロボットがクラスタ内にいるかチェック
        if (!this.isInCluster(robot, contour.cluster)) {
            return null;
        }
        
        // 輪郭（凸包）の各点から、ロボットへの方向を計算
        // 最も外側方向（輪郭から離れる方向）を選択
        let bestDirection = null;
        let maxDistance = -Infinity;
        
        for (let i = 0; i < contour.hull.length; i++) {
            const p1 = contour.hull[i];
            const p2 = contour.hull[(i + 1) % contour.hull.length];
            
            // 輪郭の辺の中点
            const midX = (p1.x + p2.x) / 2;
            const midY = (p1.y + p2.y) / 2;
            
            // ロボットから輪郭の中点への方向
            const dx = midX - robot.x;
            const dy = midY - robot.y;
            const dist = Math.hypot(dx, dy);
            
            if (dist > maxDistance) {
                maxDistance = dist;
                // 外側方向（輪郭から離れる方向）を計算
                // 輪郭の法線方向を計算（簡易版：ロボットから輪郭への方向の逆）
                const angle = Math.atan2(dy, dx);
                const normalAngle = angle + Math.PI; // 180度回転（外側方向）
                
                bestDirection = {
                    x: Math.cos(normalAngle),
                    y: Math.sin(normalAngle),
                    strength: Params.clustering.escapeForce * (1.0 + dist / 100.0) // 距離に応じて強度を調整
                };
            }
        }
        
        return bestDirection;
    }
}

