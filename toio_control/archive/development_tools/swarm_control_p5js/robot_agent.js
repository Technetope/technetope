// 個別ロボットエージェント

class RobotAgent {
    constructor(x, y, index) {
        this.x = x;
        this.y = y;
        this.angle = 0;
        this.vx = 0;
        this.vy = 0;
        this.index = index;
        
        // 自律性のための状態
        this.phase = index * 2 * Math.PI / Params.robot.count; // 位相オフセット
        
        // 速度の平滑化用（振動を減らす）
        this.smoothedVx = 0;
        this.smoothedVy = 0;
        
        // 蛇行用の状態（ランダムな方向変化）
        this.serpentineTargetAngle = 0;  // 目標方向オフセット
        this.serpentineCurrentAngle = 0;  // 現在の方向オフセット
        this.serpentineTimer = Math.random() * Params.serpentine.changeInterval; // 次に方向を変えるまでの時間
        
        // 探索用の状態
        this.targetPatch = null;  // 現在の目標パッチ
        this.explorationDirection = Math.random() * 2 * Math.PI;  // 探索方向
        this.explorationTimer = Params.autonomy.randomWalkInterval * 0.5 + Math.random() * Params.autonomy.randomWalkInterval * 0.5;  // 探索方向を変えるタイマー（初期化）
        
        // 速度分布を実装（20%速い、60%中程度、20%遅い、バイアス付きランダムシード）
        const speedCategory = this.assignSpeedCategory(index, Params.robot.count);
        this.speedCategory = speedCategory; // 速度カテゴリを保存
        
        // 速度カテゴリに基づいて基本速度を設定（よりダイナミックに）
        if (speedCategory === 'fast') {
            // 20%: 速い（maxSpeed * 1.3 - 1.7）
            this.basePreferredSpeed = Params.autonomy.maxSpeed * (1.3 + Math.random() * 0.4); // 1.2-1.5 → 1.3-1.7
        } else if (speedCategory === 'moderate') {
            // 60%: 中程度（minSpeed + (maxSpeed - minSpeed) * 0.3 から maxSpeed * 0.9）
            const moderateMin = Params.autonomy.minSpeed + (Params.autonomy.maxSpeed - Params.autonomy.minSpeed) * 0.3; // 0.4 → 0.3
            const moderateMax = Params.autonomy.maxSpeed * 0.9; // 0.8 → 0.9
            this.basePreferredSpeed = moderateMin + Math.random() * (moderateMax - moderateMin);
        } else {
            // 20%: 遅い（minSpeed * 0.7 - 1.0）
            this.basePreferredSpeed = Params.autonomy.minSpeed * (0.7 + Math.random() * 0.3); // 0.5-0.8 → 0.7-1.0
        }
        
        // 各ロボットで異なるバイアス（速度カテゴリ内でも個体差）
        this.speedBias = 0.8 + Math.random() * 0.4; // 0.8-1.2（個体差）
        this.preferredSpeed = this.basePreferredSpeed * this.speedBias;
        
        // 各ロボットに個別のパラメータを設定（個体差を出す）
        // 動的速度変化用の状態（各ロボットで異なる）
        this.speedLevel = Math.floor(Math.random() * 3); // 0, 1, 2の3レベル
        this.speedChangeInterval = 1.5 + Math.random() * 2.5; // 1.5-4秒（個体差）
        this.speedChangeTimer = Math.random() * this.speedChangeInterval;
        this.baseSpeed = this.preferredSpeed;
        this.speedVariation = 0.1 + Math.random() * 0.2; // 10-30%の変動（個体差）
        
        // 恒常性（homeostasis）用の状態（各ロボットで異なる、エントロピー導入版）
        // 20%のロボットが低い恒常性（人間の足の周りを動き回る）
        const isLowHomeostasis = (index / Params.robot.count) < 0.2; // 最初の20%
        this.isLowHomeostasis = isLowHomeostasis;
        
        this.homeostasisEnergy = isLowHomeostasis ? 
            (0.2 + Math.random() * 0.3) : // 低い恒常性：0.2-0.5
            (0.5 + Math.random() * 0.5); // 通常：0.5-1.0
        this.homeostasisTarget = isLowHomeostasis ?
            (0.3 + Math.random() * 0.2) : // 低い恒常性：0.3-0.5
            (0.6 + Math.random() * 0.3); // 通常：0.6-0.9
        this.homeostasisDecay = isLowHomeostasis ?
            (0.01 + Math.random() * 0.01) : // 低い恒常性：より速く減衰
            (0.005 + Math.random() * 0.01); // 通常
        this.homeostasisThreshold = isLowHomeostasis ?
            (0.1 + Math.random() * 0.1) : // 低い恒常性：より敏感
            (0.2 + Math.random() * 0.2); // 通常
        
        // エントロピー用の状態
        this.entropy = 0.5; // エントロピー値（0-1）
        this.entropyHistory = []; // エントロピーの履歴
        this.entropyHistorySize = 10; // 履歴のサイズ
        
        // 位置履歴追跡（同じ場所に留まらないように）
        this.positionHistory = []; // 位置の履歴
        this.positionHistorySize = 30; // 履歴のサイズ（約0.5秒、60fps想定）
        this.stationaryThreshold = 30.0; // 静止判定の閾値（mm、個体差）
        this.stationaryTime = 0; // 静止している時間
        this.stationaryTimeThreshold = 1.0 + Math.random() * 1.0; // 1-2秒（個体差）
        this.forceMoveTimer = 0; // 強制移動のタイマー
        
        // 動的パラメータ変更用の状態
        this.dynamicParameterTimer = 0; // パラメータ変更のタイマー
        this.dynamicParameterInterval = 2.0 + Math.random() * 3.0; // 2-5秒（個体差）
        this.parameterVariation = 0.1 + Math.random() * 0.2; // パラメータ変動幅（個体差）
        
        // ロボットの相互作用用の状態
        this.interactionRadius = 500.0; // 相互作用半径（50cm、個体差）
        this.interactionTimer = 0; // 相互作用チェックのタイマー
        this.interactionInterval = 0.5 + Math.random() * 0.5; // 0.5-1秒（個体差、計算コストを考慮）
        this.sharedState = {
            entropy: 0.5,
            averageSpeed: this.preferredSpeed,
            averageEnergy: this.homeostasisEnergy
        };
        
        // 彷徨（wandering）行動用の状態（各ロボットで異なる）
        this.wanderingMode = false;
        this.wanderingTimer = 0;
        this.wanderingDuration = 2.0 + Math.random() * 5.0; // 2-7秒（個体差）
        this.wanderingDirection = Math.random() * 2 * Math.PI;
        this.wanderingProbability = 0.005 + Math.random() * 0.01; // 0.005-0.015（個体差）
        
        // 向きベースの移動用の状態（各ロボットで異なる、物理的制約を反映、回転を減らす）
        this.targetOrientation = this.angle; // 目標向き
        this.orientationSpeed = 0; // 向き変更速度（rad/s）
        // 最大角速度を30-60度/秒に制限（物理的制約：バランスを保つため）
        this.maxOrientationSpeed = (Math.PI / 6) + Math.random() * (Math.PI / 6); // 30-60度/秒（個体差）
        // 速度分布に応じて向きの一致度を調整（20%以外は基本的に動く）
        if (speedCategory === 'fast') {
            // 20%: 能動的に動く（向きが完全に合っていなくても移動）
            this.orientationAlignment = -0.2 + Math.random() * 0.2; // -0.2-0.0（個体差、常に移動）
        } else if (speedCategory === 'moderate') {
            // 60%: 基本的に動く（向きが合っていなくても移動）
            this.orientationAlignment = -0.1 + Math.random() * 0.2; // -0.1-0.1（個体差、基本的に移動）
        } else {
            // 20%: 遅い（向きが合ってから移動、ただし閾値は低く）
            this.orientationAlignment = 0.2 + Math.random() * 0.2; // 0.2-0.4（個体差、低い閾値）
        }
        this.orientationSmoothing = 0.3 + Math.random() * 0.4; // 0.3-0.7（個体差）
        this.isRotating = false; // 回転中かどうか（基本的にfalse、回転は最小限）
        this.rotationStopTimer = 0; // 回転前の停止時間（削除、停止しない）
        this.rotationStopDuration = 0; // 回転前の停止時間を削除
        
        // コーナー探索用の状態（各ロボットで異なる）
        this.cornerExplorationTimer = 5.0 + Math.random() * 10.0; // 5-15秒（個体差）
        this.cornerTarget = null;
        
        // 停止と移動の繰り返しパターン（各ロボットで異なる）
        this.isMoving = true; // 現在移動中か
        this.moveDuration = 1.0 + Math.random() * 3.0; // 移動継続時間（1-4秒、個体差）
        this.stopDuration = 0.5 + Math.random() * 2.0; // 停止継続時間（0.5-2.5秒、個体差）
        this.moveStopTimer = Math.random() * this.moveDuration; // 現在のタイマー
        this.minShakeAmount = 0.1 + Math.random() * 0.2; // 最小揺れ（0.1-0.3mm、個体差）
        
        // 最適化を排除：段階的な速度変化（実際のロボットのように、よりダイナミックに）
        this.accelerationRate = 40.0 + Math.random() * 50.0; // 40-90 mm/s^2（個体差、20-50 → 40-90）
        this.decelerationRate = 50.0 + Math.random() * 60.0; // 50-110 mm/s^2（個体差、30-70 → 50-110）
        this.currentSpeed = 0; // 現在の実際の速度（段階的に変化）
        
        // 2輪ロボットの差動駆動用の状態
        this.wheelBase = 70.0; // 車輪間距離（mm、toioの実際のサイズに基づく）
        this.leftWheelSpeed = 0; // 左車輪速度（mm/s）
        this.rightWheelSpeed = 0; // 右車輪速度（mm/s）
        this.maxWheelSpeed = Params.autonomy.maxSpeed * 1.5; // 最大車輪速度
        
        // 相関ランダムウォーク用の状態
        this.currentHeading = Math.random() * 2 * Math.PI;  // 現在の進行方向
        this.headingPersistence = Params.autonomy.persistenceTime * (1.0 + Math.random() * 1.0);  // 方向保持の残り時間（長くする）
        this.lastHeadingChange = 0;  // 最後に方向を変えた時刻
        
        // 急旋回用の状態
        this.sharpTurnTimer = 3.0 + Math.random() * 4.0;  // 急旋回チェック用タイマー（長くする：3-7秒）
        
        // 局所最小値脱出用の状態
        this.lastPositionX = x;
        this.lastPositionY = y;
        this.stuckTimer = 0;  // 同じ場所に留まっている時間
        this.stuckThreshold = 2.0;  // 2秒間動かなければ脱出モード
        
        // 理想距離を設定（40-70cmの範囲内で個別に設定）
        this.idealDistance = Params.autonomy.preferredDistanceMin + 
                            Math.random() * (Params.autonomy.preferredDistanceMax - Params.autonomy.preferredDistanceMin);
        
        // 初期速度を設定
        const initialAngle = Math.random() * 2 * Math.PI;
        this.vx = Math.cos(initialAngle) * this.preferredSpeed;
        this.vy = Math.sin(initialAngle) * this.preferredSpeed;
    }
    
    // 速度カテゴリを割り当て（20%速い、60%中程度、20%遅い）
    assignSpeedCategory(index, totalCount) {
        const ratio = index / totalCount;
        if (ratio < 0.2) {
            return 'fast'; // 最初の20%
        } else if (ratio < 0.8) {
            return 'moderate'; // 次の60%
        } else {
            return 'slow'; // 最後の20%
        }
    }
    
    // 2輪ロボットの差動駆動を計算（左車輪速度と右車輪速度から角速度と線速度を計算）
    computeDifferentialDrive(linearVelocity, angularVelocity) {
        // 線速度と角速度から左車輪速度と右車輪速度を計算
        // linearVelocity = (leftWheelSpeed + rightWheelSpeed) / 2
        // angularVelocity = (rightWheelSpeed - leftWheelSpeed) / wheelBase
        // 逆算：
        // leftWheelSpeed = linearVelocity - (angularVelocity * wheelBase) / 2
        // rightWheelSpeed = linearVelocity + (angularVelocity * wheelBase) / 2
        
        const leftWheelSpeed = linearVelocity - (angularVelocity * this.wheelBase) / 2;
        const rightWheelSpeed = linearVelocity + (angularVelocity * this.wheelBase) / 2;
        
        // 最大速度で制限
        const maxSpeed = this.maxWheelSpeed;
        const leftClamped = Math.max(-maxSpeed, Math.min(maxSpeed, leftWheelSpeed));
        const rightClamped = Math.max(-maxSpeed, Math.min(maxSpeed, rightWheelSpeed));
        
        // 実際の線速度と角速度を再計算（制限後の値）
        const actualLinearVelocity = (leftClamped + rightClamped) / 2;
        const actualAngularVelocity = (rightClamped - leftClamped) / this.wheelBase;
        
        this.leftWheelSpeed = leftClamped;
        this.rightWheelSpeed = rightClamped;
        
        return {
            linearVelocity: actualLinearVelocity,
            angularVelocity: actualAngularVelocity
        };
    }
    
    // 速度ベクトルをロボットの向きに投影（前進/後進のみ、2輪ロボットの制約）
    projectVelocityToOrientation(vx, vy) {
        // ロボットの向きに基づいて速度ベクトルを投影
        const forwardX = Math.cos(this.angle);
        const forwardY = Math.sin(this.angle);
        
        // 速度ベクトルを前進方向に投影
        const projectedSpeed = vx * forwardX + vy * forwardY;
        
        // 前進方向の速度ベクトル（前進/後進のみ）
        return {
            vx: forwardX * projectedSpeed,
            vy: forwardY * projectedSpeed,
            speed: projectedSpeed
        };
    }
    
    // 位置を更新（2輪ロボットの差動駆動を反映）
    update(dt) {
        // 2輪ロボットの制約：速度ベクトルをロボットの向きに投影（前進/後進のみ）
        const projected = this.projectVelocityToOrientation(this.vx, this.vy);
        
        // 速度の平滑化を減らすが、完全には排除しない（実際のロボットの慣性を反映）
        // 各ロボットで異なる平滑化係数（個体差）
        const smoothing = 0.3 + Math.random() * 0.3; // 0.3-0.6（個体差）
        this.smoothedVx = smoothing * this.smoothedVx + (1 - smoothing) * projected.vx;
        this.smoothedVy = smoothing * this.smoothedVy + (1 - smoothing) * projected.vy;
        
        // 平滑化された速度で位置を更新
        this.x += this.smoothedVx * dt;
        this.y += this.smoothedVy * dt;
        
        // 最小限の揺れを追加（周波数ベースではなく、ランダムな微小変動のみ）
        this.applyMinimalShaking(dt);
        
        // 角度を更新（2輪ロボットの差動駆動：角速度から計算）
        const speed = Math.hypot(this.smoothedVx, this.smoothedVy);
        if (speed > 1.0) {
            // 角速度を計算（差動駆動から）
            const linearVel = speed;
            const targetAngle = Math.atan2(this.smoothedVy, this.smoothedVx);
            let angleDiff = targetAngle - this.angle;
            while (angleDiff > Math.PI) angleDiff -= 2 * Math.PI;
            while (angleDiff < -Math.PI) angleDiff += 2 * Math.PI;
            
            // 角速度を計算（差動駆動モデル）
            const angularVel = angleDiff / dt;
            const drive = this.computeDifferentialDrive(linearVel, angularVel);
            
            // 角度を更新（角速度から）
            this.angle += drive.angularVelocity * dt;
            
            // 角度を0-2πの範囲に正規化
            while (this.angle < 0) this.angle += 2 * Math.PI;
            while (this.angle >= 2 * Math.PI) this.angle -= 2 * Math.PI;
        }
    }
    
    // 動的速度変化を更新（各ロボットで異なるパラメータ、最適化を排除）
    updateDynamicSpeed(dt) {
        this.speedChangeTimer -= dt;
        if (this.speedChangeTimer <= 0) {
            // 速度レベルをランダムに変更（0: 遅い, 1: 普通, 2: 速い）
            this.speedLevel = Math.floor(Math.random() * 3);
            this.speedChangeTimer = this.speedChangeInterval; // 個体差のある間隔
            
            // レベルに応じて基本速度を変更（個体差を考慮）
            if (this.speedLevel === 0) {
                this.baseSpeed = Params.autonomy.minSpeed * (0.5 + Math.random() * 0.3); // 遅い（個体差）
            } else if (this.speedLevel === 1) {
                this.baseSpeed = (Params.autonomy.minSpeed + Params.autonomy.maxSpeed) / 2 * (0.8 + Math.random() * 0.4); // 普通（個体差）
            } else {
                this.baseSpeed = Params.autonomy.maxSpeed * (1.0 + Math.random() * 0.4); // 速い（個体差）
            }
        }
        
        // ランダムな速度変動を毎フレーム適用（より動的に）
        // 基本速度に個体差のある変動を加える
        const randomVariation = 1.0 - this.speedVariation + Math.random() * this.speedVariation * 2;
        
        // 個体差を強調（速度誤差を大きくする）
        const individualVariation = 0.7 + Math.random() * 0.6; // 0.7-1.3（個体差を大きく）
        const targetBaseSpeed = this.baseSpeed * randomVariation * individualVariation;
        
        // 段階的な速度変化（実際のロボットのように、最適化を排除）
        // currentSpeedを段階的にtargetBaseSpeedに近づける
        const speedDiff = targetBaseSpeed - this.currentSpeed;
        
        if (Math.abs(speedDiff) > 1.0) {
            // 加速または減速
            const rate = speedDiff > 0 ? this.accelerationRate : this.decelerationRate;
            const speedChange = Math.sign(speedDiff) * Math.min(Math.abs(speedDiff), rate * dt);
            this.currentSpeed += speedChange;
        } else {
            this.currentSpeed = targetBaseSpeed;
        }
        
        // preferredSpeedをcurrentSpeedに基づいて更新（段階的な変化を保持、個体差を強調）
        const speedError = 0.85 + Math.random() * 0.3; // 0.85-1.15（個体差、毎フレーム変動）
        this.preferredSpeed = Math.max(Params.autonomy.minSpeed, 
                                       Math.min(Params.autonomy.maxSpeed, 
                                              this.currentSpeed * speedError));
    }
    
    // エントロピーを計算（位置の分散度を測定）
    calculateEntropy(neighbors) {
        if (!neighbors || neighbors.length === 0) return 0.5; // デフォルト値
        
        // 近傍ロボットの位置の分散を計算
        let sumX = 0, sumY = 0;
        for (const neighbor of neighbors) {
            sumX += neighbor.x;
            sumY += neighbor.y;
        }
        const avgX = sumX / neighbors.length;
        const avgY = sumY / neighbors.length;
        
        // 分散を計算
        let variance = 0;
        for (const neighbor of neighbors) {
            const dx = neighbor.x - avgX;
            const dy = neighbor.y - avgY;
            variance += dx * dx + dy * dy;
        }
        variance /= neighbors.length;
        
        // エントロピーを正規化（0-1の範囲）
        const maxVariance = 100000.0; // 最大分散（フィールドサイズに基づく）
        const entropy = Math.min(1.0, variance / maxVariance);
        
        return entropy;
    }
    
    // 動的パラメータ変更（非線形関数の収束を防ぐ）
    updateDynamicParameters(dt, neighbors) {
        this.dynamicParameterTimer -= dt;
        
        if (this.dynamicParameterTimer <= 0) {
            // エントロピーを計算
            if (neighbors && neighbors.length > 0) {
                this.entropy = this.calculateEntropy(neighbors);
                this.entropyHistory.push(this.entropy);
                if (this.entropyHistory.length > this.entropyHistorySize) {
                    this.entropyHistory.shift();
                }
            }
            
            // エントロピーが低い場合（収束している）、パラメータを変更
            const avgEntropy = this.entropyHistory.length > 0 ?
                this.entropyHistory.reduce((a, b) => a + b, 0) / this.entropyHistory.length : 0.5;
            
            if (avgEntropy < 0.3) {
                // エントロピーが低い：パラメータをランダムに変更（収束を防ぐ）
                const variation = (Math.random() - 0.5) * this.parameterVariation;
                this.homeostasisTarget = Math.max(0.1, Math.min(1.0, this.homeostasisTarget + variation));
                this.homeostasisDecay = Math.max(0.001, Math.min(0.02, this.homeostasisDecay + variation * 0.001));
                this.homeostasisThreshold = Math.max(0.05, Math.min(0.5, this.homeostasisThreshold + variation * 0.05));
                
                // 回転を減らすため、向きの一致度も動的に変更（よりダイナミックに）
                const orientationVariation = (Math.random() - 0.5) * 0.1; // ±0.1
                this.orientationAlignment = Math.max(0.3, Math.min(0.9, this.orientationAlignment + orientationVariation));
            }
            
            // エントロピーが高い場合（分散している）、パラメータを安定化
            if (avgEntropy > 0.7) {
                // エントロピーが高い：パラメータを少し安定化（過度な動きを防ぐ）
                const stabilization = (Math.random() - 0.5) * this.parameterVariation * 0.5; // 変動を半分に
                this.orientationAlignment = Math.max(0.3, Math.min(0.9, this.orientationAlignment + stabilization * 0.1));
            }
            
            this.dynamicParameterTimer = this.dynamicParameterInterval;
        }
    }
    
    // 恒常性（homeostasis）を更新（エントロピー導入版、50cm以内の動的移動）
    updateHomeostasis(dt, neighbors) {
        // 動的パラメータ変更
        this.updateDynamicParameters(dt, neighbors);
        
        // 位置履歴を更新（同じ場所に留まらないように）
        this.positionHistory.push({x: this.x, y: this.y, time: Date.now()});
        if (this.positionHistory.length > this.positionHistorySize) {
            this.positionHistory.shift(); // 古い履歴を削除
        }
        
        // 50cm以内のロボットとの距離をチェック（恒常性の定義）
        let tooClose = false;
        let closestDistance = Infinity;
        if (neighbors && neighbors.length > 0) {
            for (const neighbor of neighbors) {
                const dx = this.x - neighbor.x;
                const dy = this.y - neighbor.y;
                const dist = Math.hypot(dx, dy);
                if (dist < 500.0) { // 50cm以内
                    tooClose = true;
                    closestDistance = Math.min(closestDistance, dist);
                }
            }
        }
        
        // 50cm以内のロボットがいる場合、離れる必要がある（動的に移動）
        if (tooClose && closestDistance < 500.0) {
            // 恒常性を破る：強制的に移動
            this.forceMoveTimer = 1.0 + Math.random() * 2.0; // 1-3秒間強制移動
            this.homeostasisEnergy = Math.max(0.1, this.homeostasisEnergy - 0.1); // エネルギーを下げる
        }
        
        // 静止検出：一定時間内の移動距離を監視
        if (this.positionHistory.length >= this.positionHistorySize) {
            const oldest = this.positionHistory[0];
            const newest = this.positionHistory[this.positionHistory.length - 1];
            const timeDiff = (newest.time - oldest.time) / 1000.0; // 秒
            
            if (timeDiff > 0.1) {
                const distance = Math.hypot(newest.x - oldest.x, newest.y - oldest.y);
                const avgSpeed = distance / timeDiff;
                
                if (avgSpeed < this.stationaryThreshold) {
                    // 静止している
                    this.stationaryTime += dt;
                    
                    // 静止時間が閾値を超えたら恒常性を破る
                    if (this.stationaryTime > this.stationaryTimeThreshold) {
                        // 強制的な移動促進：ランダムな方向に移動を強制
                        this.forceMoveTimer = 2.0 + Math.random() * 2.0; // 2-4秒間強制移動
                        this.stationaryTime = 0; // リセット
                        
                        // ランダムな方向に強制移動
                        const randomAngle = Math.random() * 2 * Math.PI;
                        this.currentHeading = randomAngle;
                        this.wanderingMode = true;
                        this.wanderingTimer = this.wanderingDuration;
                        
                        // エネルギーを急激に下げる（恒常性を破る）
                        this.homeostasisEnergy = 0.1;
                    }
                } else {
                    // 動いている
                    this.stationaryTime = 0;
                }
            }
        }
        
        // エネルギーを減衰させる（個体差のある減衰率）
        this.homeostasisEnergy -= this.homeostasisDecay * dt;
        
        // 目標値からの偏差を計算
        const deviation = this.homeostasisTarget - this.homeostasisEnergy;
        
        // 偏差が大きい場合は探索行動を促進（個体差のある閾値）
        if (Math.abs(deviation) > this.homeostasisThreshold) {
            // エネルギーが低い場合は探索を促進
            this.wanderingMode = true;
            this.wanderingTimer = this.wanderingDuration;
        }
        
        // 彷徨中はエネルギーを回復（個体差のある回復率）
        if (this.wanderingMode) {
            const recoveryRate = 0.01 + Math.random() * 0.02; // 個体差
            this.homeostasisEnergy += recoveryRate * dt;
        }
        
        // 強制移動中はエネルギーを回復
        if (this.forceMoveTimer > 0) {
            this.forceMoveTimer -= dt;
            const recoveryRate = 0.02 + Math.random() * 0.03; // 個体差、より速く回復
            this.homeostasisEnergy += recoveryRate * dt;
        }
    }
    
    // 彷徨（wandering）行動を更新（各ロボットで異なるパラメータ）
    updateWandering(dt) {
        if (this.wanderingMode) {
            this.wanderingTimer -= dt;
            if (this.wanderingTimer <= 0) {
                this.wanderingMode = false;
                this.wanderingDirection = Math.random() * 2 * Math.PI; // 新しい方向
            } else {
                // 彷徨中はランダムな方向に移動（個体差のある角度変化）
                const angleChangeRange = 0.3 + Math.random() * 0.4; // 0.3-0.7ラジアン（個体差）
                const angleChange = (Math.random() - 0.5) * angleChangeRange;
                this.wanderingDirection += angleChange;
            }
        } else {
            // 一定確率で彷徨モードに入る（個体差のある確率）
            if (Math.random() < this.wanderingProbability * dt * 60) {
                this.wanderingMode = true;
                this.wanderingTimer = this.wanderingDuration;
                this.wanderingDirection = Math.random() * 2 * Math.PI;
            }
        }
    }
    
    // 近傍ロボットを取得（距離内、計算コストを考慮）
    getNeighborsWithinDistance(robots, maxDistance) {
        const neighbors = [];
        const maxDistSq = maxDistance * maxDistance;
        
        for (const robot of robots) {
            if (robot === this) continue;
            const dx = this.x - robot.x;
            const dy = this.y - robot.y;
            const distSq = dx * dx + dy * dy;
            if (distSq < maxDistSq) {
                neighbors.push(robot);
            }
        }
        
        return neighbors;
    }
    
    // ロボットの相互作用を更新（50cm以内の相互作用、状態の共有、計算コストを考慮）
    updateRobotInteraction(dt, neighbors) {
        this.interactionTimer -= dt;
        
        // 計算コストを考慮：一定間隔でのみ相互作用をチェック
        if (this.interactionTimer <= 0) {
            if (neighbors && neighbors.length > 0) {
                // 近傍ロボットの状態を集約
                let sumEntropy = 0;
                let sumSpeed = 0;
                let sumEnergy = 0;
                let count = 0;
                
                for (const neighbor of neighbors) {
                    if (neighbor.sharedState) {
                        sumEntropy += neighbor.sharedState.entropy || 0.5;
                        sumSpeed += neighbor.sharedState.averageSpeed || neighbor.preferredSpeed;
                        sumEnergy += neighbor.sharedState.averageEnergy || neighbor.homeostasisEnergy;
                        count++;
                    }
                }
                
                if (count > 0) {
                    // 近傍ロボットの平均状態を計算
                    const avgEntropy = sumEntropy / count;
                    const avgSpeed = sumSpeed / count;
                    const avgEnergy = sumEnergy / count;
                    
                    // 共有状態を更新（エントロピーに基づいて調整）
                    this.sharedState.entropy = 0.7 * this.entropy + 0.3 * avgEntropy; // 自分の状態と近傍の平均を合成
                    this.sharedState.averageSpeed = 0.7 * this.preferredSpeed + 0.3 * avgSpeed;
                    this.sharedState.averageEnergy = 0.7 * this.homeostasisEnergy + 0.3 * avgEnergy;
                    
                    // エントロピーが低い場合（収束している）、パラメータを調整
                    if (this.sharedState.entropy < 0.3) {
                        // 速度を少し変更（収束を防ぐ）
                        const speedVariation = (Math.random() - 0.5) * 20.0; // ±20mm/s
                        this.preferredSpeed = Math.max(Params.autonomy.minSpeed, 
                                                     Math.min(Params.autonomy.maxSpeed, 
                                                            this.preferredSpeed + speedVariation));
                        
                        // 回転を減らすため、向きの一致度も動的に変更（よりダイナミックに）
                        const orientationVariation = (Math.random() - 0.5) * 0.05; // ±0.05
                        this.orientationAlignment = Math.max(0.3, Math.min(0.9, this.orientationAlignment + orientationVariation));
                    }
                    
                    // エントロピーが高い場合（分散している）、パラメータを安定化
                    if (this.sharedState.entropy > 0.7) {
                        // エントロピーが高い：パラメータを少し安定化（過度な動きを防ぐ）
                        const stabilization = (Math.random() - 0.5) * 0.02; // 小さな変動
                        this.orientationAlignment = Math.max(0.3, Math.min(0.9, this.orientationAlignment + stabilization));
                    }
                } else {
                    // 近傍ロボットがない場合、自分の状態を共有
                    this.sharedState.entropy = this.entropy;
                    this.sharedState.averageSpeed = this.preferredSpeed;
                    this.sharedState.averageEnergy = this.homeostasisEnergy;
                }
            } else {
                // 近傍ロボットがない場合、自分の状態を共有
                this.sharedState.entropy = this.entropy;
                this.sharedState.averageSpeed = this.preferredSpeed;
                this.sharedState.averageEnergy = this.homeostasisEnergy;
            }
            
            this.interactionTimer = this.interactionInterval;
        }
    }
    
    // 人間の動きを詳細に反映（立っている場合：20%のロボットが5台に限定、50cm以内、40-70cm範囲で探索）
    findPathAroundHumanFeet(humanSpots, robots) {
        if (!humanSpots || humanSpots.length === 0) return null;
        
        // 人間が立っているか動いているかを判定
        for (const spot of humanSpots) {
            const humanSpeed = Math.hypot(spot.velocityX, spot.velocityY);
            const isStanding = humanSpeed < Params.potential.staticThreshold; // 静止判定
            
            const headToFoot = Params.potential.headToFootDistance;
            const footSep = Params.potential.footSeparation;
            const footRadius = Params.potential.footRadius;
            
            // 頭の進行方向を考慮して足の位置を計算
            let heading;
            if (humanSpeed > 1.0) {
                heading = Math.atan2(spot.velocityY, spot.velocityX);
            } else {
                heading = Math.PI / 2; // デフォルトは下方向
            }
            const perpHeading = heading + Math.PI / 2.0;
            
            // 左右の足の位置
            const leftFootX = spot.headX + Math.cos(perpHeading) * (footSep / 2) + Math.cos(heading) * headToFoot;
            const leftFootY = spot.headY + Math.sin(perpHeading) * (footSep / 2) + Math.sin(heading) * headToFoot;
            const rightFootX = spot.headX - Math.cos(perpHeading) * (footSep / 2) + Math.cos(heading) * headToFoot;
            const rightFootY = spot.headY - Math.sin(perpHeading) * (footSep / 2) + Math.sin(heading) * headToFoot;
            
            // ロボットから足の間の中心点への距離
            const centerX = (leftFootX + rightFootX) / 2;
            const centerY = (leftFootY + rightFootY) / 2;
            const distToCenter = Math.hypot(this.x - centerX, this.y - centerY);
            
            // 人間への接近を許可（相互作用範囲を広げる：1m以内）
            if (distToCenter < 1000.0) {
                // 一部のロボット（30%）は人間に近づく
                const shouldApproach = Math.random() < 0.3;
                if (shouldApproach && distToCenter > 300.0 && distToCenter < 1000.0) {
                    // 300mm-1000mmの範囲で人間に近づく
                    const dx = centerX - this.x;
                    const dy = centerY - this.y;
                    const dist = Math.hypot(dx, dy);
                    
                    if (dist > 10.0) {
                        return {
                            x: dx / dist,
                            y: dy / dist,
                            distance: dist
                        };
                    }
                }
            }
            
            if (isStanding) {
                // 人間が立っている場合：20%のロボット（低い恒常性）が50cm以内で5台に限定
                if (this.isLowHomeostasis && distToCenter < 500.0) {
                    // 50cm以内のロボットをカウント（低い恒常性のロボットのみ）
                    let nearbyLowHomeostasisCount = 0;
                    if (robots && robots.length > 0) {
                        for (const robot of robots) {
                            if (robot === this) continue;
                            if (robot.isLowHomeostasis) {
                                const dx = robot.x - centerX;
                                const dy = robot.y - centerY;
                                const dist = Math.hypot(dx, dy);
                                if (dist < 500.0) {
                                    nearbyLowHomeostasisCount++;
                                }
                            }
                        }
                    }
                    
                    // 5台に限定
                    if (nearbyLowHomeostasisCount < 5) {
                        // 40-70cm範囲で探索
                        const targetDist = 400.0 + Math.random() * 300.0; // 40-70cm
                        const angle = Math.atan2(this.y - centerY, this.x - centerX);
                        const targetX = centerX + Math.cos(angle) * targetDist;
                        const targetY = centerY + Math.sin(angle) * targetDist;
                        
                        const dx = targetX - this.x;
                        const dy = targetY - this.y;
                        const dist = Math.hypot(dx, dy);
                        
                        if (dist > 10.0) {
                            return {
                                x: dx / dist,
                                y: dy / dist,
                                distance: dist
                            };
                        }
                    }
                }
            } else {
                // 人間が動いている場合：すべてのロボットが足を避ける
                if (distToCenter < 500.0 && footSep > 150.0) {
                    // 足の間を通り抜ける経路を計算
                    const dx = centerX - this.x;
                    const dy = centerY - this.y;
                    const dist = Math.hypot(dx, dy);
                    
                    if (dist > 10.0) {
                        return {
                            x: dx / dist,
                            y: dy / dist,
                            centerX: centerX,
                            centerY: centerY,
                            distance: dist
                        };
                    }
                }
            }
        }
        
        return null;
    }
    
    // 向きベースの移動を更新（まず向きを変更してから移動、物理的制約を反映）
    updateOrientationBasedMovement(dt, targetDirection) {
        if (!targetDirection) return;
        
        // 目標方向の角度を計算
        const targetAngle = Math.atan2(targetDirection.y, targetDirection.x);
        
        // 現在の向きと目標向きの差を計算
        let angleDiff = targetAngle - this.angle;
        
        // 角度を-πからπの範囲に正規化
        while (angleDiff > Math.PI) angleDiff -= 2 * Math.PI;
        while (angleDiff < -Math.PI) angleDiff += 2 * Math.PI;
        
        // 回転の閾値を非常に大きくする（ほぼ回転しない、移動を優先）
        // 20%以外は基本的に動くので、回転は最小限（144度以上の場合のみ回転）
        const threshold = Math.PI * 0.8; // 144度以上の場合のみ回転（90度 → 144度、さらに大きく）
        
        if (Math.abs(angleDiff) > threshold) {
            // 非常に大きな角度差の場合のみ回転（最小限）
            this.isRotating = true;
            
            // 段階的な向き変更（物理的制約を反映：最大角速度30-60度/秒）
            const angularSpeed = Math.sign(angleDiff) * Math.min(Math.abs(angleDiff) / dt, this.maxOrientationSpeed);
            const angleChange = angularSpeed * dt;
            
            // 平滑化を適用（個体差のある平滑化係数）
            this.angle += angleChange * (1.0 - this.orientationSmoothing) + 
                         (targetAngle - this.angle) * this.orientationSmoothing * dt * 2.0;
            
            // 角度を0-2πの範囲に正規化
            while (this.angle < 0) this.angle += 2 * Math.PI;
            while (this.angle >= 2 * Math.PI) this.angle -= 2 * Math.PI;
        } else {
            // 90度未満の角度差は移動しながら調整（回転しない、移動を優先）
            // 小さな角度差は移動しながら調整
            const smallAngleChange = angleDiff * 0.1; // 移動しながらゆっくり調整（0.3 → 0.1、より小さく）
            this.angle += smallAngleChange;
            this.isRotating = false; // 回転中ではない
            
            // 角度を0-2πの範囲に正規化
            while (this.angle < 0) this.angle += 2 * Math.PI;
            while (this.angle >= 2 * Math.PI) this.angle -= 2 * Math.PI;
        }
    }
    
    // コーナー探索を更新（各ロボットで異なる）
    updateCornerExploration(dt) {
        this.cornerExplorationTimer -= dt;
        if (this.cornerExplorationTimer <= 0) {
            // ランダムにコーナーを選択
            const corners = [
                {x: Params.field.minX + 50, y: Params.field.minY + 50},
                {x: Params.field.maxX - 50, y: Params.field.minY + 50},
                {x: Params.field.maxX - 50, y: Params.field.maxY - 50},
                {x: Params.field.minX + 50, y: Params.field.maxY - 50}
            ];
            this.cornerTarget = corners[Math.floor(Math.random() * corners.length)];
            this.cornerExplorationTimer = 5.0 + Math.random() * 10.0; // 5-15秒（個体差）
        }
    }
    
    // 停止と移動の繰り返しパターンを更新（15秒に一回くらい止まる）
    updateMoveStopPattern(dt) {
        this.moveStopTimer -= dt;
        
        if (this.isMoving) {
            // 移動中
            if (this.moveStopTimer <= 0) {
                // 移動時間が終わったら停止判定
                // 15秒に一回くらい止まる（7%の確率で停止）
                const shouldStop = Math.random() < 0.07; // 7%の確率で停止（15秒に一回くらい）
                if (shouldStop) {
                    this.isMoving = false;
                    this.stopDuration = 1.0 + Math.random() * 2.0; // 1-3秒停止
                    this.moveStopTimer = this.stopDuration;
                } else {
                    // 停止しない場合は移動を継続
                    this.moveDuration = 10.0 + Math.random() * 10.0; // 10-20秒移動（15秒に一回くらい停止）
                    this.moveStopTimer = this.moveDuration;
                }
            }
        } else {
            // 停止中
            if (this.moveStopTimer <= 0) {
                // 停止時間が終わったら移動開始
                this.isMoving = true;
                this.moveDuration = 10.0 + Math.random() * 10.0; // 10-20秒移動（15秒に一回くらい停止）
                this.moveStopTimer = this.moveDuration;
            }
        }
    }
    
    // 最小限の揺れを追加（周波数ベースではなく、ランダムな微小変動）
    applyMinimalShaking(dt) {
        // 周波数ベースではなく、ランダムな微小変動のみ
        if (!this.isMoving) {
            // 停止中は非常に小さな揺れ（実際のロボットの微小な振動）
            const shakeX = (Math.random() - 0.5) * this.minShakeAmount;
            const shakeY = (Math.random() - 0.5) * this.minShakeAmount;
            this.x += shakeX * dt;
            this.y += shakeY * dt;
        }
    }
    
    // 自律性（相関ランダムウォーク + 急旋回）を更新
    updateAutonomy(dt, positivePatches, humanSpots) {
        const theta = Params.autonomy.theta;
        const sqrtDt = Math.sqrt(Math.max(0, dt));
        
        // 停止と移動の繰り返しパターンを更新
        this.updateMoveStopPattern(dt);
        
        // 動的速度変化を更新
        this.updateDynamicSpeed(dt);
        
        // 近傍ロボットを取得（50cm以内、計算コストを考慮）
        const neighbors = robots ? this.getNeighborsWithinDistance(robots, 500.0) : []; // 50cm以内
        
        // ロボットの相互作用を更新（50cm以内の相互作用、状態の共有、計算コストを考慮）
        this.updateRobotInteraction(dt, neighbors);
        
        // 恒常性を更新（エントロピー導入版、近傍ロボットを渡す）
        this.updateHomeostasis(dt, neighbors);
        
        // 彷徨行動を更新
        this.updateWandering(dt);
        
        // コーナー探索を更新
        this.updateCornerExploration(dt);
        
        // 局所最小値検出：移動距離をチェック
        const moveDistance = Math.hypot(this.x - this.lastPositionX, this.y - this.lastPositionY);
        if (moveDistance < 20.0) {  // 20mm未満の移動
            this.stuckTimer += dt;
        } else {
            this.stuckTimer = 0;  // 動いていればリセット
            this.lastPositionX = this.x;
            this.lastPositionY = this.y;
        }
        
        // 局所最小値から脱出：一定時間動かなければ強制的に方向を変える
        let shouldEscape = false;
        let escapeDirection = null; // 輪郭検出による脱出方向
        
        if (this.stuckTimer > this.stuckThreshold) {
            shouldEscape = true;
            
            // 輪郭検出による脱出方向を試みる（デッドロック解放）
            if (this.clusterDetector && robots) {
                const contours = this.clusterDetector.estimateContours(robots);
                for (const contour of contours) {
                    const direction = this.clusterDetector.getEscapeDirection(this, contour);
                    if (direction) {
                        escapeDirection = direction;
                        break; // 最初に見つかった脱出方向を使用
                    }
                }
            }
            
            if (escapeDirection) {
                // 輪郭の外側方向に移動
                this.currentHeading = Math.atan2(escapeDirection.y, escapeDirection.x);
                this.headingPersistence = Params.autonomy.persistenceTime * (1.5 + Math.random() * 1.0);
            } else {
                // 輪郭検出が失敗した場合は完全にランダムな方向に変更（大胆な動き）
                this.currentHeading = Math.random() * 2 * Math.PI;
                this.headingPersistence = Params.autonomy.persistenceTime * (1.5 + Math.random() * 1.0);
            }
            
            this.stuckTimer = 0;  // リセット
            this.lastPositionX = this.x;
            this.lastPositionY = this.y;
        }
        
        // 急旋回チェック（頻度を大幅に減らす）
        this.sharpTurnTimer -= dt;
        let shouldSharpTurn = false;
        if (this.sharpTurnTimer <= 0 && !shouldEscape) {
            if (Math.random() < Params.autonomy.sharpTurnProbability * 0.3) {  // 確率を3分の1に
                shouldSharpTurn = true;
                // 急旋回の角度を決定（±90度）
                const turnAngle = (Math.random() - 0.5) * 2 * Params.autonomy.sharpTurnAngle;
                this.currentHeading += turnAngle;
                this.headingPersistence = Params.autonomy.persistenceTime * (1.5 + Math.random() * 1.0); // 長く保持
            }
            this.sharpTurnTimer = 5.0 + Math.random() * 5.0; // 5-10秒ごとにチェック（長くする）
        }
        
        // 相関ランダムウォーク：方向保持時間を更新
        this.headingPersistence -= dt;
        this.lastHeadingChange += dt;
        
        // 方向を変更する必要があるかチェック（頻度を減らす）
        if (this.headingPersistence <= 0 && !shouldSharpTurn && !shouldEscape) {
            // 相関ランダムウォーク：前の方向から小さな角度変化（より小さく）
            const angleChange = this.randomGaussian() * Params.autonomy.persistenceAngle * 0.5;  // 角度変化を半分に
            this.currentHeading += angleChange;
            this.headingPersistence = Params.autonomy.persistenceTime * (1.5 + Math.random() * 1.5); // 長く保持（1.5-3倍）
            this.lastHeadingChange = 0;
        }
        
        // 人間からの距離を考慮した探索（範囲内でも分散力を追加）
        let targetDirection = null;
        let targetDistance = Infinity;
        let preferredDistance = null;
        
        if (humanSpots && humanSpots.length > 0) {
            for (const spot of humanSpots) {
                const dx = this.x - spot.headX;
                const dy = this.y - spot.headY;
                const distToHuman = Math.hypot(dx, dy);
                
                // 40cm以内のロボットは強制的に移動（20%以外は基本的に動く）
                if (distToHuman < 400.0) {
                    // 20%以外は強制的に離れる方向に移動
                    if (this.speedCategory !== 'slow') {
                        // 80%のロボットは強制的に移動
                        preferredDistance = {
                            x: dx / distToHuman,
                            y: dy / distToHuman,
                            strength: 2.0 + (400.0 - distToHuman) / 400.0 // 強い力（2.0-3.0）
                        };
                    } else {
                        // 20%のロボットは弱い力
                        preferredDistance = {
                            x: dx / distToHuman,
                            y: dy / distToHuman,
                            strength: 1.0 + (400.0 - distToHuman) / 400.0
                        };
                    }
                } else {
                    // 理想距離からの偏差を計算
                    const deviation = distToHuman - this.idealDistance;
                    const minDist = Params.autonomy.preferredDistanceMin;
                    const maxDist = Params.autonomy.preferredDistanceMax;
                    
                    if (distToHuman < minDist) {
                        // 近すぎる場合は離れる方向（強い力）
                        preferredDistance = {
                            x: dx / distToHuman,
                            y: dy / distToHuman,
                            strength: 1.0 + (minDist - distToHuman) / minDist
                        };
                    } else if (distToHuman > maxDist) {
                        // 遠すぎる場合は近づく方向（強い力）
                        preferredDistance = {
                            x: -dx / distToHuman,
                            y: -dy / distToHuman,
                            strength: 1.0 + (distToHuman - maxDist) / maxDist
                        };
                    } else {
                        // 範囲内でも理想距離に向かう弱い力を追加（分散を促す）
                        const deviationRatio = Math.abs(deviation) / ((maxDist - minDist) / 2);
                        if (deviationRatio > 0.1) { // 10%以上の偏差がある場合
                            const direction = deviation > 0 ? -1 : 1; // 理想距離に近づく方向
                            preferredDistance = {
                                x: direction * dx / distToHuman,
                                y: direction * dy / distToHuman,
                                strength: 0.3 * deviationRatio // 弱い力（30%まで）
                            };
                        }
                    }
                }
            }
        }
        
        // 正ポテンシャルパッチの探索を無効化（人間の周りを回るだけになるため）
        // プラスパッチは使用しない：負のポテンシャル（人間からの反発）と自律的な動きだけで分散
        
        // 人間の動きを詳細に反映（立っている場合：20%のロボットが5台に限定、50cm以内、40-70cm範囲で探索）
        const footPath = this.findPathAroundHumanFeet(humanSpots, robots);
        
        // 目標方向を決定（足の間の経路 > コーナー探索 > 距離維持 > 彷徨 > 相関ランダムウォーク）
        // preferredSpeedを直接使用し、状況に応じてスケール調整（ランダムな速度調整を保持）
        let finalDirection = null;
        let speedMultiplier = 1.0; // 速度の倍率（preferredSpeedを保持）
        
        if (footPath && footPath.distance < 400.0) {
            // 足の間を通り抜ける（優先度最高）
            finalDirection = footPath;
            speedMultiplier = 1.3; // 少し速く
        } else if (this.cornerTarget) {
            // コーナー探索（優先度2）
            const dx = this.cornerTarget.x - this.x;
            const dy = this.cornerTarget.y - this.y;
            const dist = Math.hypot(dx, dy);
            if (dist > 30.0) { // 30mm以上離れている場合
                finalDirection = {
                    x: dx / dist,
                    y: dy / dist
                };
                speedMultiplier = 0.9; // コーナー探索中は少し遅く
            } else {
                // コーナーに到達したら次のコーナーを選択
                this.cornerTarget = null;
            }
        } else if (preferredDistance) {
            // 距離維持
            finalDirection = preferredDistance;
            speedMultiplier = 0.7 + preferredDistance.strength * 0.3;
        } else if (this.wanderingMode) {
            // 彷徨モード
            finalDirection = {
                x: Math.cos(this.wanderingDirection),
                y: Math.sin(this.wanderingDirection)
            };
            speedMultiplier = 0.8; // 彷徨中は少し遅く
        } else {
            // 相関ランダムウォーク
            finalDirection = {
                x: Math.cos(this.currentHeading),
                y: Math.sin(this.currentHeading)
            };
            speedMultiplier = 1.0; // そのまま
        }
        
        // finalDirectionがnullの場合のフォールバック
        if (!finalDirection) {
            finalDirection = {
                x: Math.cos(this.currentHeading),
                y: Math.sin(this.currentHeading)
            };
            speedMultiplier = 1.0;
        }
        
        // preferredSpeedに倍率を適用（ランダムな速度調整を保持）
        let finalSpeed = this.preferredSpeed * speedMultiplier;
        
        // 個体差を追加（各ロボットで異なる速度誤差）
        const speedError = 0.85 + Math.random() * 0.3; // 0.85-1.15（個体差、毎フレーム変動）
        finalSpeed *= speedError;
        
        // 向きベースの移動を更新（まず向きを変更してから移動）
        this.updateOrientationBasedMovement(dt, finalDirection);
        
        // 向きに基づいて速度を設定（20%以外は基本的に動く、回転は最小限）
        if (finalDirection) {
            // 20%以外は基本的に動く（向きが合っていなくても移動）
            // 位置決定はマスターで行うので、ロボットは基本的に動き続ける
            const currentDirection = {
                x: Math.cos(this.angle),
                y: Math.sin(this.angle)
            };
            const alignment = currentDirection.x * finalDirection.x + currentDirection.y * finalDirection.y;
            
            // 速度分布に応じて速度を調整（20%以外は基本的に動く）
            if (this.speedCategory === 'fast') {
                // 20%: 能動的に動く（向きが合っていなくても移動、速度はほぼ維持）
                if (alignment > this.orientationAlignment) {
                    finalDirection = currentDirection; // 現在の向きを使用
                } else {
                    // 向きが合っていなくても移動（速度を少し下げるだけ）
                    finalSpeed *= 0.85 + Math.random() * 0.1; // 85-95%（個体差、ほぼ維持）
                }
            } else if (this.speedCategory === 'moderate') {
                // 60%: 基本的に動く（向きが合っていなくても移動）
                if (alignment > this.orientationAlignment) {
                    finalDirection = currentDirection; // 現在の向きを使用
                } else {
                    // 向きが合っていなくても移動（速度を少し下げる）
                    finalSpeed *= 0.75 + Math.random() * 0.15; // 75-90%（個体差、基本的に動く）
                }
            } else {
                // 20%: 遅い（向きが合ってから移動、ただし閾値は低い）
                if (alignment > this.orientationAlignment) {
                    finalDirection = currentDirection; // 現在の向きを使用
                } else {
                    // 向きが合っていない場合は速度を下げる
                    finalSpeed *= 0.5 + Math.random() * 0.2; // 50-70%（個体差）
                }
            }
        }
        
        // 停止と移動のパターンを適用
        let effectiveSpeed = finalSpeed;
        if (!this.isMoving) {
            // 停止中は速度を0に
            effectiveSpeed = 0;
        }
        
        // 回転中でも速度を維持（止まらない、回転は最小限なので、ほとんど発生しない）
        // 20%以外は基本的に動くので、回転中の速度制限も緩和
        if (this.isRotating) {
            // 速度分布に応じて回転中の速度を調整（速度を下げすぎない）
            if (this.speedCategory === 'fast') {
                effectiveSpeed *= 0.95; // 能動的なロボットは回転中でも95%の速度を維持（0.9 → 0.95）
            } else if (this.speedCategory === 'moderate') {
                effectiveSpeed *= 0.90; // 中程度のロボットは90%の速度を維持（0.85 → 0.90）
            } else {
                effectiveSpeed *= 0.85; // 遅いロボットは85%の速度を維持（0.7 → 0.85）
            }
        }
        
        // 強制移動中は速度を上げる
        if (this.forceMoveTimer > 0) {
            effectiveSpeed *= 2.0; // 強制移動中は速度を200%に（1.5 → 2.0、よりダイナミックに）
        }
        
        // 速度を更新（最適化を排除：段階的な変化、実際のロボットのように）
        // finalDirectionがnullでないことを確認
        if (finalDirection) {
            const targetVx = finalDirection.x * effectiveSpeed;
            const targetVy = finalDirection.y * effectiveSpeed;
            
            // 段階的な速度変化（最適化を排除、実際のロボットの物理的制約を反映）
            const vxDiff = targetVx - this.vx;
            const vyDiff = targetVy - this.vy;
            const speedDiff = Math.hypot(vxDiff, vyDiff);
            
            if (speedDiff > 1.0) {
                // 加速または減速（個体差のある加速度）
                // 停止時は減速を速く、移動開始時は加速を速く
                const rate = speedDiff > 0 ? 
                    (this.isMoving ? this.accelerationRate : this.decelerationRate * 2.0) : 
                    this.decelerationRate * 1.5;
                const maxChange = rate * dt;
                const change = Math.min(speedDiff, maxChange);
                const scale = change / speedDiff;
                
                this.vx += vxDiff * scale;
                this.vy += vyDiff * scale;
            } else {
                this.vx = targetVx;
                this.vy = targetVy;
            }
        } else {
            // finalDirectionがnullの場合、現在の速度を維持または減速
            this.vx *= 0.95;
            this.vy *= 0.95;
        }
        
        // ノイズを追加（局所最小値脱出時は大きく、通常時は小さく、個体差）
        const noiseScale = shouldEscape ? 1.5 : (0.1 + Math.random() * 0.2);  // 0.1-0.3（個体差）
        const noiseX = this.randomGaussian() * Params.autonomy.sigma * sqrtDt * noiseScale;
        const noiseY = this.randomGaussian() * Params.autonomy.sigma * sqrtDt * noiseScale;
        this.vx += noiseX;
        this.vy += noiseY;
        
        // 局所最小値脱出時は速度を強制的に上げる
        if (shouldEscape) {
            const currentSpeed = Math.hypot(this.vx, this.vy);
            if (currentSpeed < this.preferredSpeed * 1.5) {
                const scale = (this.preferredSpeed * 1.5) / currentSpeed;
                this.vx *= scale;
                this.vy *= scale;
            }
        }
        
        // 速度制限（ランダム速度を考慮）
        const speed = Math.hypot(this.vx, this.vy);
        const maxSpeed = Math.max(this.preferredSpeed, Params.autonomy.speedLimit);
        if (speed > maxSpeed && maxSpeed > 0) {
            const scale = maxSpeed / speed;
            this.vx *= scale;
            this.vy *= scale;
        }
        
        // 現在の進行方向を更新（相関ランダムウォーク用）
        if (speed > 1.0) {
            this.currentHeading = Math.atan2(this.vy, this.vx);
        }
    }
    
    // 蛇行の更新（ランダムな方向変化）
    updateSerpentine(dt) {
        this.serpentineTimer -= dt;
        
        // 一定間隔でランダムに方向を変える
        if (this.serpentineTimer <= 0) {
            // 新しい目標方向オフセットを設定（小さな変化）
            this.serpentineTargetAngle = (Math.random() - 0.5) * 2 * Params.serpentine.angleChange;
            this.serpentineTimer = Params.serpentine.changeInterval + (Math.random() - 0.5) * 0.5; // 少しランダムに
        }
        
        // 現在の方向オフセットを目標に向かって平滑化
        const smoothing = Params.serpentine.smoothing;
        this.serpentineCurrentAngle = smoothing * this.serpentineCurrentAngle + 
                                      (1 - smoothing) * this.serpentineTargetAngle;
    }
    
    // 蛇行による方向オフセットを取得
    getSerpentineOffset() {
        return this.serpentineCurrentAngle;
    }
    
    // 境界反射を適用
    applyBoundaryReflection() {
        const left = Params.field.minX + Params.boundary.margin;
        const right = Params.field.maxX - Params.boundary.margin;
        const top = Params.field.minY + Params.boundary.margin;
        const bottom = Params.field.maxY - Params.boundary.margin;
        
        if ((this.x <= left && this.vx < 0) || (this.x >= right && this.vx > 0)) {
            this.vx = -this.vx * Params.boundary.damping;
        }
        
        if ((this.y <= top && this.vy < 0) || (this.y >= bottom && this.vy > 0)) {
            this.vy = -this.vy * Params.boundary.damping;
        }
    }
    
    // 衝突回避（ロボット間）- 改善版（距離を縮小、停止信号を追加、混雑度回避と予測回避を統合）
    applyCollisionAvoidance(otherRobots, dt, densityGrid, predictiveAvoidance) {
        const accelerations = { ax: 0, ay: 0 };
        const safeDist = Params.collision.safeDistance;
        const repulsionGain = Params.collision.repulsionGain;
        const minSeparation = Params.collision.minSeparation;
        const separationForce = Params.collision.separationForce;
        const emergencyStopDist = Params.collision.emergencyStopDistance;
        const emergencyStopSpeed = Params.collision.emergencyStopSpeed;
        
        // 20%の能動的なロボット（fastカテゴリ）は予測・協調的な回避を無視
        const isActiveRobot = this.speedCategory === 'fast';
        
        // 混雑度回避の力（Social Force Modelベース）
        let crowdAvoidanceX = 0;
        let crowdAvoidanceY = 0;
        
        if (!isActiveRobot && densityGrid) {
            // 混雑度が最も低い方向を取得
            const leastCrowded = densityGrid.getLeastCrowdedDirection(this.x, this.y, Params.densityGrid.sampleRadius);
            
            if (leastCrowded && leastCrowded.density > 0) {
                // 混雑度が高い方向から反発力を計算
                const densities = densityGrid.getDirectionalDensities(this.x, this.y, Params.densityGrid.sampleRadius);
                
                for (const dir of densities) {
                    if (dir.density > 0) {
                        // 反発力の強度 = 混雑度 × 距離減衰関数
                        const strength = dir.density * Params.densityGrid.repulsionGain;
                        crowdAvoidanceX -= Math.cos(dir.angle) * strength;
                        crowdAvoidanceY -= Math.sin(dir.angle) * strength;
                    }
                }
                
                // 正規化
                const crowdLength = Math.hypot(crowdAvoidanceX, crowdAvoidanceY);
                if (crowdLength > 0.1) {
                    crowdAvoidanceX /= crowdLength;
                    crowdAvoidanceY /= crowdLength;
                    const crowdStrength = Math.min(crowdLength, Params.densityGrid.repulsionGain);
                    crowdAvoidanceX *= crowdStrength;
                    crowdAvoidanceY *= crowdStrength;
                }
            }
        }
        
        // 緊急停止モードのフラグ
        let isEmergencyStop = false;
        
        for (const other of otherRobots) {
            if (other === this) continue;
            
            const dx = this.x - other.x;
            const dy = this.y - other.y;
            const dist = Math.hypot(dx, dy);
            
            // 最悪の場合（衝突直前、71mm以下）は停止信号を送る
            if (dist < emergencyStopDist && dist > 0.001) {
                // 緊急停止モード：速度を大幅に減速（10-20%に）し、反発方向に移動
                isEmergencyStop = true;
                
                // 速度を大幅に減速
                this.vx *= emergencyStopSpeed;
                this.vy *= emergencyStopSpeed;
                
                // 強力な反発力（反発方向に移動）
                const strength = separationForce * 2.0 * (1.0 / dist - 1.0 / emergencyStopDist);
                const fx = strength * (dx / dist);
                const fy = strength * (dy / dist);
                accelerations.ax += fx;
                accelerations.ay += fy;
                
                // 最小限の位置修正（衝突を防ぐため、動きを制限しない）
                const overlap = emergencyStopDist - dist;
                if (overlap > 0) {
                    const separationX = (dx / dist) * overlap * 0.3; // 0.5 → 0.3（弱める）
                    const separationY = (dy / dist) * overlap * 0.3;
                    this.x += separationX;
                    this.y += separationY;
                }
            } else if (dist < minSeparation && dist > 0.001) {
                // 最小分離距離より近い場合（弱い位置修正）
                // 強制的な位置分離を弱める（動きを制限しない）
                const overlap = minSeparation - dist;
                const separationX = (dx / dist) * overlap * 0.2; // 0.5 → 0.2（大幅に弱める）
                const separationY = (dy / dist) * overlap * 0.2;
                
                // 位置を直接分離（弱く）
                this.x += separationX;
                this.y += separationY;
                
                // 反発力も追加（弱める）
                const strength = separationForce * (1.0 / dist - 1.0 / minSeparation);
                const fx = strength * (dx / dist);
                const fy = strength * (dy / dist);
                accelerations.ax += fx;
                accelerations.ay += fy;
            } else if (dist < safeDist && dist > 0.001) {
                // 通常の反発力
                const strength = repulsionGain * (1.0 / dist - 1.0 / safeDist);
                const fx = strength * (dx / dist);
                const fy = strength * (dy / dist);
                accelerations.ax += fx;
                accelerations.ay += fy;
            }
        }
        
        // 境界反発
        const left = Params.field.minX;
        const right = Params.field.maxX;
        const top = Params.field.minY;
        const bottom = Params.field.maxY;
        const boundaryGain = Params.collision.boundaryGain;
        
        const distLeft = this.x - left;
        const distRight = right - this.x;
        const distTop = this.y - top;
        const distBottom = bottom - this.y;
        
        if (distLeft < safeDist && distLeft > 0.001) {
            accelerations.ax += boundaryGain * (1.0 / distLeft - 1.0 / safeDist);
        }
        if (distRight < safeDist && distRight > 0.001) {
            accelerations.ax -= boundaryGain * (1.0 / distRight - 1.0 / safeDist);
        }
        if (distTop < safeDist && distTop > 0.001) {
            accelerations.ay += boundaryGain * (1.0 / distTop - 1.0 / safeDist);
        }
        if (distBottom < safeDist && distBottom > 0.001) {
            accelerations.ay -= boundaryGain * (1.0 / distBottom - 1.0 / safeDist);
        }
        
        // 予測的衝突回避の力（能動的なロボットは無視）
        let predictiveX = 0;
        let predictiveY = 0;
        
        if (!isActiveRobot && predictiveAvoidance) {
            const predictiveForces = predictiveAvoidance.applyPredictiveAvoidance(this, otherRobots);
            predictiveX = predictiveForces.fx;
            predictiveY = predictiveForces.fy;
        }
        
        // 能動的なロボットは回避を無視（最小限の衝突回避のみ）
        if (isActiveRobot) {
            // 最小限の衝突回避のみ（緊急停止距離のみ）
            this.vx += accelerations.ax * dt;
            this.vy += accelerations.ay * dt;
            return;
        }
        
        // 混雑度回避、予測回避、衝突回避を重み付けして合成
        const weightCrowd = Params.densityGrid.weightCrowdAvoidance;
        const weightPredictive = Params.densityGrid.weightPredictive;
        const weightCollision = Params.densityGrid.weightCollision;
        
        // 各力を正規化
        const crowdLength = Math.hypot(crowdAvoidanceX, crowdAvoidanceY);
        if (crowdLength > 0.1) {
            crowdAvoidanceX /= crowdLength;
            crowdAvoidanceY /= crowdLength;
        }
        
        const predictiveLength = Math.hypot(predictiveX, predictiveY);
        if (predictiveLength > 0.1) {
            predictiveX /= predictiveLength;
            predictiveY /= predictiveLength;
        }
        
        const collisionLength = Math.hypot(accelerations.ax, accelerations.ay);
        if (collisionLength > 0.1) {
            accelerations.ax /= collisionLength;
            accelerations.ay /= collisionLength;
        }
        
        // 各力の強度を計算（元の強度を保持）
        const crowdStrength = Math.hypot(crowdAvoidanceX, crowdAvoidanceY) * Params.densityGrid.repulsionGain;
        const predictiveStrength = Math.hypot(predictiveX, predictiveY) * Params.predictive.repulsionGain;
        const collisionStrength = collisionLength;
        
        // 重み付けして合成
        const combinedAx = (crowdAvoidanceX * crowdStrength * weightCrowd +
                           predictiveX * predictiveStrength * weightPredictive +
                           accelerations.ax * collisionStrength * weightCollision);
        const combinedAy = (crowdAvoidanceY * crowdStrength * weightCrowd +
                           predictiveY * predictiveStrength * weightPredictive +
                           accelerations.ay * collisionStrength * weightCollision);
        
        // 加速度を速度に加算
        this.vx += combinedAx * dt;
        this.vy += combinedAy * dt;
    }
    
    // 衝突ブレーキ
    applyCollisionBrake(otherRobots) {
        const stopDist = Params.collision.stopDistance;
        const minScale = Params.collision.minScale;
        let scale = 1.0;
        
        for (const other of otherRobots) {
            if (other === this) continue;
            
            const dx = this.x - other.x;
            const dy = this.y - other.y;
            const dist = Math.hypot(dx, dy);
            
            if (dist < stopDist) {
                const ratio = dist / stopDist;
                const factor = Math.max(minScale, Math.min(1.0, ratio));
                scale = Math.min(scale, factor);
            }
        }
        
        this.vx *= scale;
        this.vy *= scale;
    }
    
    // 速度制限
    applySpeedLimit() {
        const speed = Math.hypot(this.vx, this.vy);
        if (speed > Params.robot.maxSpeed && Params.robot.maxSpeed > 0) {
            const scale = Params.robot.maxSpeed / speed;
            this.vx *= scale;
            this.vy *= scale;
        }
        // 最小速度を確保（動き続けるため）- ただし、速度が0に近い場合は適用しない
        const minSpeed = 20.0; // mm/s
        if (speed > 0.1 && speed < minSpeed) {
            const scale = minSpeed / speed;
            this.vx *= scale;
            this.vy *= scale;
        }
    }
    
    // 位置をクランプ
    clampPosition() {
        this.x = Math.max(Params.field.minX, Math.min(Params.field.maxX, this.x));
        this.y = Math.max(Params.field.minY, Math.min(Params.field.maxY, this.y));
    }
    
    // 描画
    draw() {
        push();
        translate(this.x, this.y);
        rotate(this.angle);
        
        // ロボット本体（71mm × 32mm）
        fill(100, 150, 255);
        stroke(255);
        strokeWeight(2);
        rect(-35.5, -16, 71, 32, 4);
        
        // 進行方向を示す矢印
        stroke(255, 200, 0);
        strokeWeight(3);
        line(0, 0, 20, 0);
        line(20, 0, 15, -5);
        line(20, 0, 15, 5);
        
        // 速度ベクトルを表示
        if (Params.visualization.showVectors) {
            stroke(0, 255, 0);
            strokeWeight(1);
            line(0, 0, this.vx * 0.1, this.vy * 0.1);
        }
        
        pop();
    }
    
    // ガウス乱数生成（簡易版）
    randomGaussian() {
        // Box-Muller変換
        const u1 = Math.random();
        const u2 = Math.random();
        return Math.sqrt(-2 * Math.log(u1)) * Math.cos(2 * Math.PI * u2);
    }
}

