// パラメータ設定（調整可能）
const Params = {
    // フィールド設定
    field: {
        minX: 34.0,      // mm
        minY: 35.0,      // mm
        maxX: 949.0,     // mm
        maxY: 898.0,     // mm
        safetyMargin: 50.0  // mm
    },
    
    // ポテンシャル場
    potential: {
        lambda: 1.0,           // ポテンシャル強度
        beta: 2.5,              // 時間減衰率
        alpha: 0.9,              // 勾配係数
        spotRadius: 200.0,      // スポットの影響半径 (mm)
        gridSize: 50.0,         // グリッド解像度 (mm)
        updateInterval: 5,       // 更新間隔（フレーム数）
        staticThreshold: 20.0,  // 人間が立っている判定の速度閾値 (mm/s)
        // 人間の足
        headToFootDistance: 900.0,  // 頭から足までの距離 (mm) - 90cm
        footSeparation: 250.0,       // 左右の足の間隔 (mm) - 25cm
        footRadius: 100.0,          // 足の影響半径 (mm) - 10cm
        footStrength: 1.5           // 足のポテンシャル強度
    },
    
    // 正ポテンシャルパッチ（分散を促す）
    positivePatches: {
        count: 30,           // パッチ数 - 増やした（30台のロボットに対して）
        radius: 70.0,        // パッチの半径 (mm) - 大きくした
        strength: 0.9,      // パッチの強度 - 上げた
        wanderSpeed: 15.0,   // ランダムウォーク速度 (mm/s)
        updateInterval: 300,  // パッチ再配置間隔（フレーム数）
        attractStrength: 1.2,  // ロボットへの吸引力
        arrivalDistance: 50.0, // 到着判定距離 (mm) - この距離内に入ったら消える
        minDistance: 400.0,   // 人間からの最小距離 (mm) - 40cm
        maxDistance: 700.0,   // 人間からの最大距離 (mm) - 70cm
        staticThreshold: 20.0, // 静止判定の速度閾値 (mm/s)
        staticPatchRadius: 150.0 // 静止時のパッチ半径 (mm)
    },
    
    // 緊急退避
    urgent: {
        threshold: 80.0,        // 緊急判定距離 (mm)
        escapeSpeed: 300.0,     // 緊急時の速度 (mm/s) - 上げた
        duration: 2000.0,       // 持続時間 (ms)
        threatSpeedThreshold: 50.0  // 脅威速度閾値 (mm/s)
    },
    
    // Boidモデル（時々群として作用）
    boid: {
        separationDistance: 80.0,   // 分離距離 (mm)
        alignmentDistance: 150.0,    // 整列距離 (mm)
        cohesionDistance: 200.0,     // 結合距離 (mm)
        separationWeight: 0.5,       // 分離の重み
        alignmentWeight: 0.3,        // 整列の重み
        cohesionWeight: 0.2,         // 結合の重み
        interactionWeight: 0.4,      // 相互作用の重み（発動時）
        autonomyWeight: 0.6,         // 自律性の重み
        activationProbability: 0.15  // 群れ行動発動確率（15%）
    },
    
    // 蛇行（ランダムな方向変化）
    serpentine: {
        changeInterval: 2.0,    // 方向変化の間隔（秒）- 頻度を少なめに
        angleChange: 0.15,      // 方向変化の最大角度（ラジアン）- 小さく
        influence: 0.3,          // 影響度（30%）
        minSpeed: 30.0,          // 適用最小速度 (mm/s)
        smoothing: 0.9           // 方向変化の平滑化
    },
    
    // 衝突回避
    collision: {
        safeDistance: 80.0,      // 安全距離 (mm) - 縮小（150mm → 80mm、toioの最長辺 + 9mm余裕）
        stopDistance: 75.0,      // 停止距離 (mm) - 縮小（120mm → 75mm）
        repulsionGain: 6000.0,   // 反発力ゲイン - さらに上げた
        boundaryGain: 3200.0,    // 境界反発力ゲイン
        minScale: 0.05,          // 最小スケール
        minSeparation: 76.0,     // 最小分離距離 (mm) - 縮小（150mm → 76mm、toioの最長辺71mm + 5mm余裕）
        separationForce: 3000.0, // 強制分離力 - 弱める（8000.0 → 3000.0）
        emergencyStopDistance: 71.0, // 緊急停止距離 (mm) - toioの最長辺と同じ
        emergencyStopSpeed: 0.15  // 緊急停止時の速度倍率（15%に減速）
    },
    
    // 多層混雑度推定
    densityGrid: {
        sampleRadius: 100.0,    // 方向性判定のサンプル半径 (mm)
        repulsionGain: 2000.0,   // 混雑度からの反発力ゲイン
        weightCrowdAvoidance: 0.4, // 混雑度回避の重み（40%）
        weightPredictive: 0.3,   // 予測回避の重み（30%）
        weightCollision: 0.3     // 衝突回避の重み（30%）
    },
    
    // 予測的衝突回避
    predictive: {
        predictionTimes: [0.5, 1.0, 2.0], // 予測時間（秒）の配列
        predictionRadius: 76.0,  // 予測領域の半径 (mm) - ロボットサイズ + 安全マージン
        repulsionGain: 1500.0,   // 予測領域からの反発力ゲイン
        uncertaintyGrowth: 0.2   // 予測時間が長いほど不確実性を大きくする係数
    },
    
    // クラスタリングと輪郭検出
    clustering: {
        clusterDistance: 100.0,  // クラスタリングの距離閾値 (mm)
        contourSampleRadius: 150.0, // 輪郭検出のサンプル半径 (mm)
        escapeForce: 3000.0      // 輪郭外側への脱出力
    },
    
    // 境界処理
    boundary: {
        margin: 60.0,           // マージン (mm)
        damping: 0.5             // 減衰係数
    },
    
    // 自律性（目標指向的探索）
    autonomy: {
        theta: 0.8,             // 減衰率
        sigma: 60.0,            // ノイズ強度（探索のためのランダム性）
        biasX: 0.0,              // 平均速度X
        biasY: 0.0,              // 平均速度Y
        speedLimit: 250.0,       // 速度制限 (mm/s) - 上げた（150.0 → 250.0）
        minSpeed: 120.0,         // 最小速度 (mm/s) - ランダム速度用、上げた（80.0 → 120.0）
        maxSpeed: 250.0,         // 最大速度 (mm/s) - ランダム速度用、上げた（150.0 → 250.0）
        smoothing: 0.6,         // 速度の平滑化係数（0-1、大きいほど滑らか）- 弱めた
        // 正ポテンシャルパッチ探索
        patchSearchRadius: 300.0,  // パッチ探索半径 (mm)
        patchAttractionWeight: 0.6, // パッチへの吸引力の重み
        explorationWeight: 0.4,      // 探索行動の重み（パッチが見つからない場合）
        explorationNoise: 100.0,     // 探索時のノイズ強度
        // 相関ランダムウォーク（よりダイナミックに）
        persistenceAngle: 0.25,      // 角度変化の標準偏差（ラジアン）- 大きく（0.15 → 0.25）
        persistenceTime: 5.0,        // 方向を保持する時間（秒）- 短く（8.0 → 5.0秒）
        // 急旋回（よりダイナミックに）
        sharpTurnProbability: 0.03,  // 急旋回の確率（3%）- 上げた（0.02 → 0.03）
        sharpTurnAngle: Math.PI / 2, // 急旋回の角度範囲（±90度）
        // 距離維持
        preferredDistanceMin: 400.0, // 好ましい距離の最小値 (mm) - 40cm
        preferredDistanceMax: 700.0, // 好ましい距離の最大値 (mm) - 70cm
        nearAreaProbability: 0.2,     // 近くのエリアを選択する確率（20%）
        // ランダムウォーク
        randomWalkInterval: 7.0,      // ランダムウォークの間隔（秒、5-10秒の範囲）
        // 動的速度変化
        speedChangeInterval: 2.0,    // 速度レベル変更間隔（秒）
        speedLevels: 3,              // 速度レベル数（0: 遅い, 1: 普通, 2: 速い）
        // 恒常性（homeostasis）
        homeostasisTarget: 0.7,      // 目標エネルギー値
        homeostasisDecay: 0.01,       // エネルギー減衰率
        homeostasisThreshold: 0.3,    // 探索行動を促進する偏差閾値
        // 彷徨（wandering）
        wanderingProbability: 0.01,  // 彷徨モードに入る確率（フレームごと）
        wanderingDuration: 3.0,       // 彷徨継続時間（秒）
        // 向きベースの移動（物理的制約を反映）
        orientationAlignment: 0.7,    // 移動を開始する向きの一致度（0-1）
        maxAngularSpeed: Math.PI / 3,  // 最大角速度（rad/s、60度/秒）- 物理的制約
        rotationStopDuration: 0.1,    // 回転前の停止時間（秒）
        // 恒常性の強化
        positionHistorySize: 30,      // 位置履歴のサイズ（フレーム数）
        stationaryThreshold: 30.0,    // 静止判定の閾値（mm/s）
        stationaryTimeThreshold: 1.0, // 静止時間の閾値（秒）
        // 速度分布
        speedDistribution: {
            fast: 0.2,      // 20%が速い
            moderate: 0.6,  // 60%が中程度
            slow: 0.2       // 20%が遅い
        }
    },
    
    // ロボット設定
    robot: {
        count: 30,               // ロボット数
        radius: 35.0,            // 衝突判定半径 (mm)
        maxSpeed: 180.0,         // 最大速度 (mm/s)
        lookaheadTime: 0.35      // 予測時間 (s)
    },
    
    // 可視化
    visualization: {
        showPotential: true,     // ポテンシャル場を表示
        showVectors: true,       // 速度ベクトルを表示
        showSpots: true,         // スポットを表示
        potentialAlpha: 0.3      // ポテンシャル場の透明度
    }
};

