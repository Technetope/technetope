// Boidモデル（群れ行動）

class BoidModel {
    constructor() {
        this.spatialHash = new Map();
        this.hashCellSize = 200.0; // 安全距離の2倍程度
    }
    
    // 空間ハッシュを更新
    updateSpatialHash(robots) {
        this.spatialHash.clear();
        
        for (let i = 0; i < robots.length; i++) {
            const robot = robots[i];
            const key = this.getHashKey(robot.x, robot.y);
            
            if (!this.spatialHash.has(key)) {
                this.spatialHash.set(key, []);
            }
            this.spatialHash.get(key).push(i);
        }
    }
    
    // ハッシュキーを取得
    getHashKey(x, y) {
        const ix = Math.floor(x / this.hashCellSize);
        const iy = Math.floor(y / this.hashCellSize);
        return `${ix},${iy}`;
    }
    
    // 近傍ロボットを取得
    getNeighbors(robotIndex, robots) {
        const robot = robots[robotIndex];
        const neighbors = [];
        
        // 現在のセルと周囲8セルをチェック
        const centerKey = this.getHashKey(robot.x, robot.y);
        const [cx, cy] = centerKey.split(',').map(Number);
        
        for (let dx = -1; dx <= 1; dx++) {
            for (let dy = -1; dy <= 1; dy++) {
                const key = `${cx + dx},${cy + dy}`;
                const cellRobots = this.spatialHash.get(key);
                if (cellRobots) {
                    for (const idx of cellRobots) {
                        if (idx !== robotIndex) {
                            neighbors.push(idx);
                        }
                    }
                }
            }
        }
        
        return neighbors;
    }
    
    // 群れ行動を適用（時々発動）
    applyFlocking(robots, robotStates, dt) {
        this.updateSpatialHash(robots);
        
        for (let i = 0; i < robots.length; i++) {
            const robot = robots[i];
            const state = robotStates[i];
            
            // 確率的に群れ行動を発動（ALife的な振る舞い）
            const shouldFlock = Math.random() < Params.boid.activationProbability;
            
            if (shouldFlock) {
                // 近傍ロボットを取得
                const neighbors = this.getNeighbors(i, robots);
                
                if (neighbors.length > 0) {
                    // 分離・整列・結合を計算
                    const separation = this.computeSeparation(i, robots, neighbors);
                    const alignment = this.computeAlignment(i, robots, robotStates, neighbors);
                    const cohesion = this.computeCohesion(i, robots, neighbors);
                    
                    // 自律性（既存の速度）
                    const autonomyX = state.vx * Params.boid.autonomyWeight;
                    const autonomyY = state.vy * Params.boid.autonomyWeight;
                    
                    // 重み付け合成
                    const flockX = Params.boid.separationWeight * separation.x +
                                  Params.boid.alignmentWeight * alignment.x +
                                  Params.boid.cohesionWeight * cohesion.x;
                    const flockY = Params.boid.separationWeight * separation.y +
                                  Params.boid.alignmentWeight * alignment.y +
                                  Params.boid.cohesionWeight * cohesion.y;
                    
                    // 相互作用と自律性のバランス（時々群として作用）
                    state.vx = Params.boid.interactionWeight * flockX + autonomyX;
                    state.vy = Params.boid.interactionWeight * flockY + autonomyY;
                }
            }
            // 発動しない場合は自律性を維持（state.vx, state.vyはそのまま）
        }
    }
    
    // 分離（Separation）
    computeSeparation(robotIndex, robots, neighbors) {
        let sepX = 0, sepY = 0;
        
        for (const j of neighbors) {
            const other = robots[j];
            const robot = robots[robotIndex];
            
            const dx = robot.x - other.x;
            const dy = robot.y - other.y;
            const dist = Math.hypot(dx, dy);
            
            if (dist < Params.boid.separationDistance && dist > 0.001) {
                const strength = 1.0 / dist;
                sepX += dx * strength;
                sepY += dy * strength;
            }
        }
        
        return { x: sepX, y: sepY };
    }
    
    // 整列（Alignment）
    computeAlignment(robotIndex, robots, robotStates, neighbors) {
        let alignX = 0, alignY = 0;
        let count = 0;
        
        for (const j of neighbors) {
            const other = robots[j];
            const robot = robots[robotIndex];
            
            const dx = robot.x - other.x;
            const dy = robot.y - other.y;
            const dist = Math.hypot(dx, dy);
            
            if (dist < Params.boid.alignmentDistance) {
                alignX += robotStates[j].vx;
                alignY += robotStates[j].vy;
                count++;
            }
        }
        
        if (count > 0) {
            alignX /= count;
            alignY /= count;
            
            // 正規化して最大速度を適用
            const alignSpeed = Math.hypot(alignX, alignY);
            if (alignSpeed > 0) {
                alignX = (alignX / alignSpeed) * Params.robot.maxSpeed;
                alignY = (alignY / alignSpeed) * Params.robot.maxSpeed;
            }
        }
        
        return { x: alignX, y: alignY };
    }
    
    // 結合（Cohesion）
    computeCohesion(robotIndex, robots, neighbors) {
        let centerX = 0, centerY = 0;
        let count = 0;
        const robot = robots[robotIndex];
        
        for (const j of neighbors) {
            const other = robots[j];
            
            const dx = robot.x - other.x;
            const dy = robot.y - other.y;
            const dist = Math.hypot(dx, dy);
            
            if (dist < Params.boid.cohesionDistance) {
                centerX += other.x;
                centerY += other.y;
                count++;
            }
        }
        
        if (count > 0) {
            centerX = (centerX / count) - robot.x;
            centerY = (centerY / count) - robot.y;
            
            // 正規化して最大速度を適用
            const cohDist = Math.hypot(centerX, centerY);
            if (cohDist > 0) {
                centerX = (centerX / cohDist) * Params.robot.maxSpeed;
                centerY = (centerY / cohDist) * Params.robot.maxSpeed;
            }
        }
        
        return { x: centerX, y: centerY };
    }
}

