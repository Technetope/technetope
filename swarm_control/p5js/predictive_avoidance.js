// ベクトルベースの将来位置予測回避（予測的衝突回避）

class PredictiveAvoidance {
    constructor() {
        this.predictionTimes = Params.predictive.predictionTimes; // [0.5, 1.0, 2.0]秒
        this.predictionRadius = Params.predictive.predictionRadius; // 76mm
        this.repulsionGain = Params.predictive.repulsionGain;
        this.uncertaintyGrowth = Params.predictive.uncertaintyGrowth;
    }
    
    // ロボットの将来位置を予測
    predictFuturePosition(robot, predictionTime) {
        // 現在位置 + 速度ベクトル × 予測時間
        const futureX = robot.x + robot.vx * predictionTime;
        const futureY = robot.y + robot.vy * predictionTime;
        
        // 予測時間が長いほど、領域の不確実性を大きくする
        const uncertainty = 1.0 + predictionTime * this.uncertaintyGrowth;
        const radius = this.predictionRadius * uncertainty;
        
        return {
            x: futureX,
            y: futureY,
            radius: radius,
            time: predictionTime
        };
    }
    
    // すべてのロボットの将来位置を予測
    predictAllFuturePositions(robots) {
        const predictions = [];
        
        for (const robot of robots) {
            const robotPredictions = [];
            for (const predTime of this.predictionTimes) {
                const futurePos = this.predictFuturePosition(robot, predTime);
                robotPredictions.push(futurePos);
            }
            predictions.push({
                robot: robot,
                predictions: robotPredictions
            });
        }
        
        return predictions;
    }
    
    // 自分の将来位置が他のロボットの予測領域に入るかチェック
    checkCollisionWithPredictions(robot, allPredictions) {
        const avoidanceForces = { fx: 0, fy: 0 };
        
        // 自分の将来位置を予測
        const myPredictions = [];
        for (const predTime of this.predictionTimes) {
            myPredictions.push(this.predictFuturePosition(robot, predTime));
        }
        
        // 他のロボットの予測領域と衝突チェック
        for (const otherPred of allPredictions) {
            if (otherPred.robot === robot) continue;
            
            // 各予測時間で衝突チェック
            for (let i = 0; i < this.predictionTimes.length; i++) {
                const myFuture = myPredictions[i];
                const otherFuture = otherPred.predictions[i];
                
                // 将来位置間の距離
                const dx = myFuture.x - otherFuture.x;
                const dy = myFuture.y - otherFuture.y;
                const dist = Math.hypot(dx, dy);
                
                // 予測領域が重なる場合（半径の合計より距離が小さい）
                const combinedRadius = myFuture.radius + otherFuture.radius;
                if (dist < combinedRadius && dist > 0.001) {
                    // 反発力を計算
                    // 反発力の強度 = 予測時間に応じた重み × 距離減衰
                    const timeWeight = 1.0 / (1.0 + this.predictionTimes[i]); // 予測時間が長いほど重みを小さく
                    const overlap = combinedRadius - dist;
                    const strength = this.repulsionGain * timeWeight * (overlap / combinedRadius);
                    
                    // 反発方向
                    const dirX = dx / dist;
                    const dirY = dy / dist;
                    
                    avoidanceForces.fx += dirX * strength;
                    avoidanceForces.fy += dirY * strength;
                }
            }
        }
        
        return avoidanceForces;
    }
    
    // ロボットに予測回避力を適用
    applyPredictiveAvoidance(robot, allRobots) {
        // すべてのロボットの将来位置を予測
        const allPredictions = this.predictAllFuturePositions(allRobots);
        
        // 衝突チェックと回避力を計算
        const avoidanceForces = this.checkCollisionWithPredictions(robot, allPredictions);
        
        return avoidanceForces;
    }
}

