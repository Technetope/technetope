// スポットベースのポテンシャル場計算

class SpotPotentialField {
    constructor() {
        this.grid = [];
        this.gridWidth = 0;
        this.gridHeight = 0;
        // Paramsが定義されているか確認してから使用
        if (typeof Params !== 'undefined' && Params.potential) {
            this.cellSize = Params.potential.gridSize;
        } else {
            this.cellSize = 50.0; // デフォルト値
        }
        this.updateCounter = 0;
        this.spots = [];
        // 正ポテンシャルパッチ
        this.positivePatches = [];
        this.patchUpdateCounter = 0;
        this.initializePositivePatches();
    }
    
    // 正ポテンシャルパッチを初期化
    initializePositivePatches() {
        this.positivePatches = [];
        // 初期パッチは人間の位置を基準に生成（spotsが空の場合は後で生成）
    }
    
    // 人間の位置を基準にパッチを生成（半径40-70cmの範囲）
    generatePatchesAroundHuman(humanSpots) {
        // Paramsが定義されているか確認
        if (typeof Params === 'undefined' || !Params.positivePatches) {
            console.warn('Params not defined, skipping patch generation');
            return;
        }
        const params = Params.positivePatches;
        const count = params.count;
        
        // 既存のパッチをクリア
        this.positivePatches = [];
        
        if (humanSpots && humanSpots.length > 0) {
            // 人間の位置を基準に生成
            for (const spot of humanSpots) {
                const humanSpeed = Math.hypot(spot.velocityX, spot.velocityY);
                const isStatic = humanSpeed < params.staticThreshold;
                
                if (isStatic) {
                    // 静止している場合：足の周りにパッチを生成
                    const staticCount = Math.floor(count * 0.3); // 30%を静止用に
                    for (let i = 0; i < staticCount; i++) {
                        const angle = Math.random() * 2 * Math.PI;
                        const distance = params.staticPatchRadius * (0.5 + Math.random() * 0.5);
                        const patch = {
                            x: spot.headX + Math.cos(angle) * distance,
                            y: spot.headY + Math.sin(angle) * distance,
                            vx: 0,
                            vy: 0,
                            phase: Math.random() * 2 * Math.PI,
                            gradient: Math.random() * 0.3 + 0.7, // 勾配の強度（0.7-1.0）
                            isStatic: true
                        };
                        // フィールド内にクランプ
                        patch.x = Math.max(Params.field.minX, Math.min(Params.field.maxX, patch.x));
                        patch.y = Math.max(Params.field.minY, Math.min(Params.field.maxY, patch.y));
                        this.positivePatches.push(patch);
                    }
                }
                
                // 半径40-70cmの範囲にランダムにパッチを生成
                const dynamicCount = count - (isStatic ? Math.floor(count * 0.3) : 0);
                for (let i = 0; i < dynamicCount; i++) {
                    const angle = Math.random() * 2 * Math.PI;
                    const distance = params.minDistance + Math.random() * (params.maxDistance - params.minDistance);
                    const patch = {
                        x: spot.headX + Math.cos(angle) * distance,
                        y: spot.headY + Math.sin(angle) * distance,
                        vx: (Math.random() - 0.5) * params.wanderSpeed,
                        vy: (Math.random() - 0.5) * params.wanderSpeed,
                        phase: Math.random() * 2 * Math.PI,
                        gradient: Math.random() * 0.4 + 0.6, // 勾配の強度（0.6-1.0、ムラを出す）
                        isStatic: false
                    };
                    // フィールド内にクランプ
                    patch.x = Math.max(Params.field.minX, Math.min(Params.field.maxX, patch.x));
                    patch.y = Math.max(Params.field.minY, Math.min(Params.field.maxY, patch.y));
                    this.positivePatches.push(patch);
                }
            }
        } else {
            // 人間がいない場合はランダムに生成
            const fieldWidth = Params.field.maxX - Params.field.minX;
            const fieldHeight = Params.field.maxY - Params.field.minY;
            for (let i = 0; i < count; i++) {
                this.positivePatches.push({
                    x: Params.field.minX + Math.random() * fieldWidth,
                    y: Params.field.minY + Math.random() * fieldHeight,
                    vx: (Math.random() - 0.5) * params.wanderSpeed,
                    vy: (Math.random() - 0.5) * params.wanderSpeed,
                    phase: Math.random() * 2 * Math.PI,
                    gradient: Math.random() * 0.4 + 0.6,
                    isStatic: false
                });
            }
        }
    }
    
    // グリッドを初期化
    initialize() {
        const fieldWidth = Params.field.maxX - Params.field.minX;
        const fieldHeight = Params.field.maxY - Params.field.minY;
        this.gridWidth = Math.ceil(fieldWidth / this.cellSize);
        this.gridHeight = Math.ceil(fieldHeight / this.cellSize);
        
        // グリッドを0で初期化
        this.grid = [];
        for (let y = 0; y < this.gridHeight; y++) {
            this.grid[y] = [];
            for (let x = 0; x < this.gridWidth; x++) {
                this.grid[y][x] = 0.0;
            }
        }
    }
    
    // スポットを更新
    updateSpots(spots, robots) {
        this.spots = spots;
        this.updateCounter++;
        this.patchUpdateCounter++;
        
        // 正ポテンシャルパッチの生成・更新を無効化
        // プラスパッチは使用しない：人間の周りを回るだけになるため、高速で切り替えても意味がない
        // 負のポテンシャル（人間からの反発）と自律的な動きだけで分散
        
        // 更新間隔をチェック
        if (this.updateCounter % Params.potential.updateInterval !== 0) {
            return;
        }
        
        // グリッドをリセット
        for (let y = 0; y < this.gridHeight; y++) {
            for (let x = 0; x < this.gridWidth; x++) {
                this.grid[y][x] = 0.0;
            }
        }
        
        // 各スポットからポテンシャルを計算（負のポテンシャルのみ）
        for (const spot of spots) {
            this.addSpotPotential(spot);
            // 人間の足のポテンシャルを追加
            this.addFootPotential(spot);
        }
        
        // 正ポテンシャルパッチは使用しない
    }
    
    // 到着したパッチを削除
    removeArrivedPatches(robots) {
        const arrivalDist = Params.positivePatches.arrivalDistance;
        this.positivePatches = this.positivePatches.filter(patch => {
            for (const robot of robots) {
                const dx = robot.x - patch.x;
                const dy = robot.y - patch.y;
                const dist = Math.hypot(dx, dy);
                if (dist < arrivalDist) {
                    return false; // 到着したので削除
                }
            }
            return true; // 保持
        });
    }
    
    // 正ポテンシャルパッチを更新
    updatePositivePatches() {
        const dt = Params.potential.updateInterval / 60.0; // フレーム間隔を秒に変換
        const wanderSpeed = Params.positivePatches.wanderSpeed;
        
        for (const patch of this.positivePatches) {
            // 静止パッチは動かさない
            if (patch.isStatic) {
                continue;
            }
            
            // ランダムウォーク（平滑化）
            patch.vx += (Math.random() - 0.5) * 20.0 * dt;
            patch.vy += (Math.random() - 0.5) * 20.0 * dt;
            
            // 速度制限
            const speed = Math.hypot(patch.vx, patch.vy);
            if (speed > wanderSpeed) {
                patch.vx = (patch.vx / speed) * wanderSpeed;
                patch.vy = (patch.vy / speed) * wanderSpeed;
            }
            
            // 位置を更新
            patch.x += patch.vx * dt;
            patch.y += patch.vy * dt;
            
            // 境界で反射
            if (patch.x < Params.field.minX || patch.x > Params.field.maxX) {
                patch.vx = -patch.vx;
                patch.x = Math.max(Params.field.minX, Math.min(Params.field.maxX, patch.x));
            }
            if (patch.y < Params.field.minY || patch.y > Params.field.maxY) {
                patch.vy = -patch.vy;
                patch.y = Math.max(Params.field.minY, Math.min(Params.field.maxY, patch.y));
            }
        }
    }
    
    // 正ポテンシャルパッチを追加（勾配をつけてムラを出す）
    addPositivePatch(patch) {
        const radius = Params.positivePatches.radius;
        const strength = Params.positivePatches.strength;
        const gradient = patch.gradient || 1.0; // 勾配の強度（0.6-1.0）
        
        // グリッド範囲を計算
        const minGridX = Math.max(0, Math.floor((patch.x - radius - Params.field.minX) / this.cellSize));
        const maxGridX = Math.min(this.gridWidth - 1, Math.ceil((patch.x + radius - Params.field.minX) / this.cellSize));
        const minGridY = Math.max(0, Math.floor((patch.y - radius - Params.field.minY) / this.cellSize));
        const maxGridY = Math.min(this.gridHeight - 1, Math.ceil((patch.y + radius - Params.field.minY) / this.cellSize));
        
        // グリッドセルにガウシアンを加算（勾配をつける）
        for (let gy = minGridY; gy <= maxGridY; gy++) {
            for (let gx = minGridX; gx <= maxGridX; gx++) {
                const worldX = Params.field.minX + (gx + 0.5) * this.cellSize;
                const worldY = Params.field.minY + (gy + 0.5) * this.cellSize;
                
                const dx = worldX - patch.x;
                const dy = worldY - patch.y;
                const dist = Math.hypot(dx, dy);
                
                if (dist < radius) {
                    // ガウシアン（正のポテンシャル）
                    const exponent = -0.5 * (dist * dist) / (radius * radius);
                    const gaussian = Math.exp(exponent);
                    
                    // 勾配をつける（距離に応じて強度が変わる、ムラを出す）
                    const normalizedDist = dist / radius;
                    const gradientFactor = gradient * (1.0 - normalizedDist * 0.3); // 中心から離れるほど弱くなる
                    
                    // 角度による勾配（位相を使ってムラを出す）
                    const angle = Math.atan2(dy, dx);
                    const phaseFactor = 1.0 + 0.2 * Math.sin(angle * 3 + patch.phase); // 3つの方向に強弱
                    
                    // 強度を上げて分散を促す
                    this.grid[gy][gx] += strength * Params.potential.lambda * gaussian * 
                                        Params.positivePatches.attractStrength * 
                                        gradientFactor * phaseFactor;
                }
            }
        }
    }
    
    // 人間の足のポテンシャルを追加
    addFootPotential(spot) {
        const headToFoot = Params.potential.headToFootDistance;
        const footSep = Params.potential.footSeparation;
        const footRadius = Params.potential.footRadius;
        const footStrength = Params.potential.footStrength;
        
        // 頭の進行方向を考慮して足の位置を計算
        const speed = Math.hypot(spot.velocityX, spot.velocityY);
        let heading;
        if (speed > 1.0) {
            heading = Math.atan2(spot.velocityY, spot.velocityX);
        } else {
            // 速度が小さい場合は、前回の方向を保持（デフォルトは下方向）
            heading = Math.PI / 2; // 下方向（Y軸正方向）
        }
        const perpHeading = heading + Math.PI / 2.0; // 進行方向に対して垂直
        
        // 左右の足の位置
        const leftFootX = spot.headX + Math.cos(perpHeading) * (footSep / 2) + Math.cos(heading) * headToFoot;
        const leftFootY = spot.headY + Math.sin(perpHeading) * (footSep / 2) + Math.sin(heading) * headToFoot;
        const rightFootX = spot.headX - Math.cos(perpHeading) * (footSep / 2) + Math.cos(heading) * headToFoot;
        const rightFootY = spot.headY - Math.sin(perpHeading) * (footSep / 2) + Math.sin(heading) * headToFoot;
        
        // 左右の足それぞれに負のポテンシャルを追加
        this.addSingleFootPotential(leftFootX, leftFootY, footRadius, footStrength);
        this.addSingleFootPotential(rightFootX, rightFootY, footRadius, footStrength);
    }
    
    // 単一の足のポテンシャルを追加
    addSingleFootPotential(footX, footY, radius, strength) {
        // グリッド範囲を計算
        const minGridX = Math.max(0, Math.floor((footX - radius - Params.field.minX) / this.cellSize));
        const maxGridX = Math.min(this.gridWidth - 1, Math.ceil((footX + radius - Params.field.minX) / this.cellSize));
        const minGridY = Math.max(0, Math.floor((footY - radius - Params.field.minY) / this.cellSize));
        const maxGridY = Math.min(this.gridHeight - 1, Math.ceil((footY + radius - Params.field.minY) / this.cellSize));
        
        // グリッドセルにガウシアンを加算（負のポテンシャル）
        for (let gy = minGridY; gy <= maxGridY; gy++) {
            for (let gx = minGridX; gx <= maxGridX; gx++) {
                const worldX = Params.field.minX + (gx + 0.5) * this.cellSize;
                const worldY = Params.field.minY + (gy + 0.5) * this.cellSize;
                
                const dx = worldX - footX;
                const dy = worldY - footY;
                const dist = Math.hypot(dx, dy);
                
                if (dist < radius) {
                    // ガウシアン（負のポテンシャル）
                    const exponent = -0.5 * (dist * dist) / (radius * radius);
                    const gaussian = Math.exp(exponent);
                    this.grid[gy][gx] -= strength * Params.potential.lambda * gaussian;
                }
            }
        }
    }
    
    // スポットのポテンシャルを追加
    addSpotPotential(spot) {
        const T_max = 0.6; // 予測時間の最大値（秒）
        const numSteps = 12; // 予測ステップ数
        const dt = T_max / numSteps;
        
        for (let step = 0; step < numSteps; step++) {
            const tau = step * dt;
            const weight = Math.exp(-Params.potential.beta * tau);
            
            // 予測位置
            const predX = spot.headX + spot.velocityX * tau;
            const predY = spot.headY + spot.velocityY * tau;
            
            // 楕円共分散（進行方向に細長い）
            const speed = Math.hypot(spot.velocityX, spot.velocityY);
            const heading = Math.atan2(spot.velocityY, spot.velocityX);
            const sigmaMajor = 30.0 + speed * 0.1; // 進行方向の不確実性
            const sigmaMinor = 20.0; // 横方向の不確実性
            
            // グリッド範囲を計算
            const influenceRadius = Params.potential.spotRadius;
            const minGridX = Math.max(0, Math.floor((predX - influenceRadius - Params.field.minX) / this.cellSize));
            const maxGridX = Math.min(this.gridWidth - 1, Math.ceil((predX + influenceRadius - Params.field.minX) / this.cellSize));
            const minGridY = Math.max(0, Math.floor((predY - influenceRadius - Params.field.minY) / this.cellSize));
            const maxGridY = Math.min(this.gridHeight - 1, Math.ceil((predY + influenceRadius - Params.field.minY) / this.cellSize));
            
            // グリッドセルにガウシアンを加算
            for (let gy = minGridY; gy <= maxGridY; gy++) {
                for (let gx = minGridX; gx <= maxGridX; gx++) {
                    const worldX = Params.field.minX + (gx + 0.5) * this.cellSize;
                    const worldY = Params.field.minY + (gy + 0.5) * this.cellSize;
                    
                    const potential = this.computeGaussianPotential(
                        worldX, worldY,
                        predX, predY,
                        sigmaMajor, sigmaMinor,
                        heading,
                        weight
                    );
                    
                    this.grid[gy][gx] += potential;
                }
            }
        }
    }
    
    // ガウシアンポテンシャルを計算
    computeGaussianPotential(x, y, centerX, centerY, sigmaMajor, sigmaMinor, angle, weight) {
        // 中心からの相対位置
        const dx = x - centerX;
        const dy = y - centerY;
        
        // 回転行列で座標変換
        const cosA = Math.cos(angle);
        const sinA = Math.sin(angle);
        const rotatedX = dx * cosA + dy * sinA;
        const rotatedY = -dx * sinA + dy * cosA;
        
        // ガウシアン計算
        const exponent = -0.5 * (
            (rotatedX * rotatedX) / (sigmaMajor * sigmaMajor) +
            (rotatedY * rotatedY) / (sigmaMinor * sigmaMinor)
        );
        
        const gaussian = Math.exp(exponent);
        return -Params.potential.lambda * weight * gaussian;
    }
    
    // 勾配を取得（双線形補間）
    getGradient(x, y) {
        // フィールド外の場合は0を返す
        if (x < Params.field.minX || x > Params.field.maxX ||
            y < Params.field.minY || y > Params.field.maxY) {
            return { gx: 0, gy: 0 };
        }
        
        // グリッド座標
        const gridX = (x - Params.field.minX) / this.cellSize;
        const gridY = (y - Params.field.minY) / this.cellSize;
        
        const gx0 = Math.floor(gridX);
        const gy0 = Math.floor(gridY);
        const gx1 = Math.min(this.gridWidth - 1, gx0 + 1);
        const gy1 = Math.min(this.gridHeight - 1, gy0 + 1);
        
        // 境界チェック
        if (gx0 < 0 || gx0 >= this.gridWidth || gy0 < 0 || gy0 >= this.gridHeight) {
            return { gx: 0, gy: 0 };
        }
        
        // 双線形補間
        const fx = gridX - gx0;
        const fy = gridY - gy0;
        
        const v00 = this.grid[gy0][gx0];
        const v10 = this.grid[gy0][gx1];
        const v01 = this.grid[gy1][gx0];
        const v11 = this.grid[gy1][gx1];
        
        const v0 = v00 * (1 - fx) + v10 * fx;
        const v1 = v01 * (1 - fx) + v11 * fx;
        const value = v0 * (1 - fy) + v1 * fy;
        
        // 中心差分で勾配を計算
        const dx = 0.5 * this.cellSize;
        const dy = 0.5 * this.cellSize;
        
        let gradX = 0, gradY = 0;
        
        if (gx0 > 0 && gx1 < this.gridWidth - 1) {
            const vLeft = this.grid[gy0][gx0 - 1] * (1 - fy) + this.grid[gy1][gx0 - 1] * fy;
            const vRight = this.grid[gy0][gx1 + 1] * (1 - fy) + this.grid[gy1][gx1 + 1] * fy;
            gradX = (vRight - vLeft) / (2 * dx);
        }
        
        if (gy0 > 0 && gy1 < this.gridHeight - 1) {
            const vTop = this.grid[gy0 - 1][gx0] * (1 - fx) + this.grid[gy0 - 1][gx1] * fx;
            const vBottom = this.grid[gy1 + 1][gx0] * (1 - fx) + this.grid[gy1 + 1][gx1] * fx;
            gradY = (vBottom - vTop) / (2 * dy);
        }
        
        return { gx: -gradX, gy: -gradY }; // 負の勾配（谷から逃げる方向）
    }
    
    // 可視化
    draw() {
        if (!Params.visualization.showPotential) return;
        
        push();
        noStroke();
        
        for (let y = 0; y < this.gridHeight; y++) {
            for (let x = 0; x < this.gridWidth; x++) {
                const worldX = Params.field.minX + (x + 0.5) * this.cellSize;
                const worldY = Params.field.minY + (y + 0.5) * this.cellSize;
                
                const potential = this.grid[y][x];
                const normalized = Math.max(-1, Math.min(1, potential / Params.potential.lambda));
                
                // 負のポテンシャル（危険）を赤、正のポテンシャル（安全）を青で表示
                if (normalized < 0) {
                    fill(255, 0, 0, Math.abs(normalized) * 255 * Params.visualization.potentialAlpha);
                } else {
                    fill(0, 0, 255, normalized * 255 * Params.visualization.potentialAlpha);
                }
                
                rect(
                    worldX - this.cellSize / 2,
                    worldY - this.cellSize / 2,
                    this.cellSize,
                    this.cellSize
                );
            }
        }
        
        pop();
    }
}

