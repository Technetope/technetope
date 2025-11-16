// p5.jsメインスケッチ

let robots = [];
let robotStates = [];
let humanSpots = [];
let potentialField;
let urgentEscape;
let boidModel;
let densityGrid; // 多層混雑度推定
let clusterDetector; // クラスタリングと輪郭検出
let predictiveAvoidance; // 予測的衝突回避

// スケール（mm to pixels）
let scaleX, scaleY;
let fieldWidth, fieldHeight;

// マウスで人間のスポットを制御
let mouseSpot = null;

function setup() {
    // キャンバスサイズを計算（フィールドを画面に収める）
    const canvasWidth = min(windowWidth - 40, 1200);
    const canvasHeight = min(windowHeight - 200, 900);
    
    createCanvas(canvasWidth, canvasHeight);
    
    // スケールを計算（アスペクト比を保持）
    fieldWidth = Params.field.maxX - Params.field.minX;
    fieldHeight = Params.field.maxY - Params.field.minY;
    
    // アスペクト比を保持してスケールを計算
    const fieldAspect = fieldWidth / fieldHeight;
    const canvasAspect = canvasWidth / canvasHeight;
    
    if (fieldAspect > canvasAspect) {
        // フィールドの方が横長：幅に合わせる
        scaleX = canvasWidth / fieldWidth;
        scaleY = scaleX; // 同じスケールを使用（アスペクト比保持）
    } else {
        // フィールドの方が縦長：高さに合わせる
        scaleY = canvasHeight / fieldHeight;
        scaleX = scaleY; // 同じスケールを使用（アスペクト比保持）
    }
    
    // 人間のスポットを初期化（マウスで制御）- 先に初期化
    mouseSpot = {
        headX: Params.field.minX + fieldWidth / 2,
        headY: Params.field.minY + fieldHeight / 2,
        velocityX: 0,
        velocityY: 0,
        urgent: false
    };
    humanSpots = [mouseSpot];
    
    // オブジェクトを初期化
    potentialField = new SpotPotentialField();
    potentialField.initialize();
    // 正ポテンシャルパッチの生成を無効化（人間の周りを回るだけになるため）
    urgentEscape = new UrgentEscape();
    boidModel = new BoidModel();
    densityGrid = new SpatialDensityGrid(); // 多層混雑度推定
    clusterDetector = new ClusterDetector(); // クラスタリングと輪郭検出
    predictiveAvoidance = new PredictiveAvoidance(); // 予測的衝突回避
    
    // ロボットを初期化
    initializeRobots();
    
    // パラメータ調整UI（p5.guiを使用、オプショナル）
    try {
        if (typeof Gui !== 'undefined') {
            createParameterGUI();
        }
    } catch (e) {
        console.log('GUI not available, continuing without parameter UI');
    }
}

function initializeRobots() {
    robots = [];
    robotStates = [];
    
    // 30台のロボットを分散して配置（中心に固まらないように）
    const spacing = Math.min(fieldWidth, fieldHeight) / Math.sqrt(Params.robot.count);
    for (let i = 0; i < Params.robot.count; i++) {
        // グリッド状に配置してから少しランダムにずらす
        const cols = Math.ceil(Math.sqrt(Params.robot.count));
        const row = Math.floor(i / cols);
        const col = i % cols;
        const baseX = Params.field.minX + (col + 0.5) * (fieldWidth / cols);
        const baseY = Params.field.minY + (row + 0.5) * (fieldHeight / cols);
        
        // ランダムに少しずらす
        const x = baseX + random(-spacing * 0.3, spacing * 0.3);
        const y = baseY + random(-spacing * 0.3, spacing * 0.3);
        
        // フィールド内にクランプ
        const clampedX = Math.max(Params.field.minX + 50, Math.min(Params.field.maxX - 50, x));
        const clampedY = Math.max(Params.field.minY + 50, Math.min(Params.field.maxY - 50, y));
        
        const robot = new RobotAgent(clampedX, clampedY, i);
        robots.push(robot);
        robotStates.push({ vx: 0, vy: 0 });
    }
}

function createParameterGUI() {
    // p5.guiが利用可能な場合のみ作成
    if (typeof Gui === 'undefined') {
        return;
    }
    
    try {
        const gui = new Gui();
        
        // ポテンシャル場パラメータ
        gui.addObject(Params.potential, {
            lambda: { min: 0, max: 5, step: 0.1 },
            alpha: { min: 0, max: 2, step: 0.1 }
        });
        
        // Boidパラメータ
        gui.addObject(Params.boid, {
            separationWeight: { min: 0, max: 3, step: 0.1 },
            alignmentWeight: { min: 0, max: 3, step: 0.1 },
            cohesionWeight: { min: 0, max: 3, step: 0.1 },
            interactionWeight: { min: 0, max: 1, step: 0.1 },
            autonomyWeight: { min: 0, max: 1, step: 0.1 }
        });
        
        // 蛇行パラメータ
        gui.addObject(Params.serpentine, {
            amplitude: { min: 0, max: 50, step: 1 },
            frequency: { min: 0, max: 2, step: 0.1 },
            influence: { min: 0, max: 0.5, step: 0.01 }
        });
        
        // 可視化設定
        gui.addObject(Params.visualization, {
            showPotential: { type: 'boolean' },
            showVectors: { type: 'boolean' },
            showSpots: { type: 'boolean' }
        });
    } catch (e) {
        console.warn('GUI creation failed:', e);
    }
}

function draw() {
    background(20, 20, 30);
    
    // マウス位置を人間のスポットに反映（座標変換の前に）
    updateMouseSpot();
    
    // 座標変換を設定
    push();
    translate(-Params.field.minX * scaleX, -Params.field.minY * scaleY);
    scale(scaleX, scaleY);
    
    // フィールドを描画（計算コスト：低）
    drawField();
    
    // ポテンシャル場を更新・描画（計算コストが高い）
    // 描画は条件付きでスキップ（計算は毎フレーム）
    potentialField.updateSpots(humanSpots, robots); // 計算のみ（毎フレーム必要）
    if (Params.visualization.showPotential) {
        potentialField.draw(); // 描画処理（重い、条件付き）
    }
    
    // 多層混雑度推定を更新
    densityGrid.update(robots);
    
    // 緊急退避をチェック
    urgentEscape.checkUrgentEscape(robots, humanSpots);
    
    // 時間ステップ
    const dt = 1.0 / 60.0; // 60fpsを想定
    
    // 各ロボットを更新
    for (let i = 0; i < robots.length; i++) {
        const robot = robots[i];
        const state = robotStates[i];
        
        // 緊急退避モードかチェック
        const urgentVel = urgentEscape.computeUrgentVelocity(i);
        if (urgentVel) {
            state.vx = urgentVel.vx;
            state.vy = urgentVel.vy;
        } else {
            // 通常モード
            
            // ポテンシャル場から勾配を取得
            const gradient = potentialField.getGradient(robot.x, robot.y);
            
            // 希望速度 = -α * 勾配
            let vxPref = -Params.potential.alpha * gradient.gx;
            let vyPref = -Params.potential.alpha * gradient.gy;
            
            // 自律性を更新（相関ランダムウォーク + 急旋回 + 距離維持）
            // プラスパッチは使用しない（nullを渡す）、robots配列を渡す
            robot.updateAutonomy(dt, null, humanSpots, robots);
            
            // 希望速度と自律性を合成（自律性をさらに強める、分散を促す）
            // ランダムな速度調整を保持するため、robot.vx/vyの速度を維持
            const prefSpeed = Math.hypot(vxPref, vyPref);
            const autonomySpeed = Math.hypot(robot.vx, robot.vy);
            
            if (prefSpeed < 10.0 && autonomySpeed < 10.0) {
                // 両方小さい場合は、自律性を強化（ランダムな速度調整を保持）
                state.vx = robot.vx * 1.2;
                state.vy = robot.vy * 1.2;
            } else {
                // 速度ベクトルの方向を合成し、速度の大きさは自律性（ランダムな速度調整を含む）を優先
                const autonomyDirection = autonomySpeed > 0.1 ? {
                    x: robot.vx / autonomySpeed,
                    y: robot.vy / autonomySpeed
                } : { x: 0, y: 0 };
                
                const prefDirection = prefSpeed > 0.1 ? {
                    x: vxPref / prefSpeed,
                    y: vyPref / prefSpeed
                } : { x: 0, y: 0 };
                
                // 方向を合成
                const combinedDirection = {
                    x: prefDirection.x * 0.3 + autonomyDirection.x * 0.7,
                    y: prefDirection.y * 0.3 + autonomyDirection.y * 0.7
                };
                const dirLength = Math.hypot(combinedDirection.x, combinedDirection.y);
                if (dirLength > 0.1) {
                    combinedDirection.x /= dirLength;
                    combinedDirection.y /= dirLength;
                }
                
                // 速度の大きさは自律性（ランダムな速度調整を含む）を使用
                state.vx = combinedDirection.x * autonomySpeed;
                state.vy = combinedDirection.y * autonomySpeed;
            }
            
            // 蛇行を更新（ランダムな方向変化）
            robot.updateSerpentine(dt);
            
            // 蛇行を適用（方向を少し変える）
            const speed = Math.hypot(state.vx, state.vy);
            if (speed > Params.serpentine.minSpeed) {
                const heading = Math.atan2(state.vy, state.vx);
                const serpentineOffset = robot.getSerpentineOffset();
                const newHeading = heading + serpentineOffset * Params.serpentine.influence;
                
                // 方向を少し変える（位置・向きを変えすぎない）
                state.vx = speed * Math.cos(newHeading);
                state.vy = speed * Math.sin(newHeading);
            }
        }
        
        // 状態をロボットに反映（群れ行動の前に）
        robot.vx = state.vx;
        robot.vy = state.vy;
    }
    
    // 群れ行動を一括適用（緊急退避中でないロボットのみ）
    boidModel.applyFlocking(robots, robotStates, dt);
    
    // 各ロボットに状態を反映
    for (let i = 0; i < robots.length; i++) {
        if (!urgentEscape.isUrgent(i)) {
            robots[i].vx = robotStates[i].vx;
            robots[i].vy = robotStates[i].vy;
        }
    }
    
    // 衝突回避と境界処理
    for (const robot of robots) {
        robot.applyCollisionAvoidance(robots, dt, densityGrid, predictiveAvoidance); // 混雑度推定と予測回避を渡す
        robot.applyCollisionBrake(robots);
        robot.applyBoundaryReflection();
        robot.applySpeedLimit();
        robot.clampPosition();
    }
    
    // 位置を更新
    for (const robot of robots) {
        robot.update(dt);
    }
    
    // 人間のスポットを描画
    if (Params.visualization.showSpots) {
        drawSpots();
    }
    
    // 正ポテンシャルパッチの描画を無効化（使用していないため）
    // if (Params.visualization.showSpots) {
    //     drawPositivePatches();
    // }
    
    // ロボットを描画
    for (const robot of robots) {
        robot.draw();
    }
    
    pop();
    
    // 情報を表示
    drawInfo();
}

function updateMouseSpot() {
    // マウス位置をフィールド座標に変換
    // push/popの外側なので、直接マウス座標を使用
    // 座標変換を考慮：translateとscaleが適用されているので、逆変換が必要
    // translate(-Params.field.minX * scaleX, -Params.field.minY * scaleY) と scale(scaleX, scaleY) が適用されている
    // したがって、マウス座標をそのままscaleX/scaleYで割ればフィールド座標になる
    const mouseXField = Params.field.minX + (mouseX / scaleX);
    const mouseYField = Params.field.minY + (mouseY / scaleY);
    
    // フィールド範囲内にクランプ
    const clampedX = Math.max(Params.field.minX, Math.min(Params.field.maxX, mouseXField));
    const clampedY = Math.max(Params.field.minY, Math.min(Params.field.maxY, mouseYField));
    
    // 速度を計算（前フレームからの差分）
    if (mouseSpot.lastX !== undefined) {
        const dt = 1.0 / 60.0;
        mouseSpot.velocityX = (clampedX - mouseSpot.lastX) / dt;
        mouseSpot.velocityY = (clampedY - mouseSpot.lastY) / dt;
    } else {
        mouseSpot.velocityX = 0;
        mouseSpot.velocityY = 0;
    }
    
    mouseSpot.headX = clampedX;
    mouseSpot.headY = clampedY;
    mouseSpot.lastX = clampedX;
    mouseSpot.lastY = clampedY;
    
    // 緊急フラグ（右クリックで設定）
    if (mouseIsPressed && mouseButton === RIGHT) {
        mouseSpot.urgent = true;
    } else {
        mouseSpot.urgent = false;
    }
}

function drawField() {
    // フィールドの境界
    stroke(100, 100, 150);
    strokeWeight(2);
    noFill();
    rect(
        Params.field.minX,
        Params.field.minY,
        fieldWidth,
        fieldHeight
    );
    
    // グリッド（オプション）
    if (false) { // グリッドを表示する場合は true に
        stroke(50, 50, 70);
        strokeWeight(1);
        for (let x = Params.field.minX; x <= Params.field.maxX; x += 100) {
            line(x, Params.field.minY, x, Params.field.maxY);
        }
        for (let y = Params.field.minY; y <= Params.field.maxY; y += 100) {
            line(Params.field.minX, y, Params.field.maxX, y);
        }
    }
}

function drawSpots() {
    for (const spot of humanSpots) {
        // スポットの中心
        fill(255, 100, 100);
        noStroke();
        circle(spot.headX, spot.headY, 20);
        
        // 影響範囲
        noFill();
        stroke(255, 100, 100, 100);
        strokeWeight(2);
        circle(spot.headX, spot.headY, Params.potential.spotRadius * 2);
        
        // 速度ベクトル
        if (Math.hypot(spot.velocityX, spot.velocityY) > 1) {
            stroke(255, 200, 100);
            strokeWeight(2);
            line(
                spot.headX,
                spot.headY,
                spot.headX + spot.velocityX * 0.1,
                spot.headY + spot.velocityY * 0.1
            );
        }
        
        // 人間の足を描画
        const headToFoot = Params.potential.headToFootDistance;
        const footSep = Params.potential.footSeparation;
        const footRadius = Params.potential.footRadius;
        
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
        
        // 足を描画（赤い円）
        fill(200, 50, 50, 200);
        stroke(255, 100, 100, 255);
        strokeWeight(2);
        circle(leftFootX, leftFootY, footRadius * 2);
        circle(rightFootX, rightFootY, footRadius * 2);
        
        // 頭から足への線
        stroke(150, 150, 150, 100);
        strokeWeight(1);
        line(spot.headX, spot.headY, leftFootX, leftFootY);
        line(spot.headX, spot.headY, rightFootX, rightFootY);
        
        // 緊急フラグ表示
        if (spot.urgent) {
            fill(255, 0, 0, 150);
            noStroke();
            circle(spot.headX, spot.headY, 100);
        }
    }
}

function drawPositivePatches() {
    const patches = potentialField.positivePatches;
    if (!patches) return;
    
    for (const patch of patches) {
        const radius = Params.positivePatches.radius;
        
        // 静止パッチと動的パッチで色を変える
        if (patch.isStatic) {
            fill(150, 200, 255, 180);
            stroke(200, 230, 255, 220);
        } else {
            fill(100, 150, 255, 150);
            stroke(150, 200, 255, 200);
        }
        strokeWeight(1);
        circle(patch.x, patch.y, radius * 2);
        
        // 中心点
        if (patch.isStatic) {
            fill(200, 230, 255);
        } else {
            fill(150, 200, 255);
        }
        noStroke();
        circle(patch.x, patch.y, 5);
    }
}

function drawInfo() {
    // 情報パネル
    fill(0, 0, 0, 200);
    noStroke();
    rect(10, 10, 300, 150);
    
    fill(255);
    textSize(14);
    textAlign(LEFT, TOP);
    text(`FPS: ${Math.round(frameRate())}`, 20, 20);
    text(`Robots: ${robots.length}`, 20, 40);
    text(`Spots: ${humanSpots.length}`, 20, 60);
    text(`Urgent: ${urgentEscape.active ? 'YES' : 'NO'}`, 20, 80);
    text(`Mouse: (${Math.round(mouseSpot.headX)}, ${Math.round(mouseSpot.headY)})`, 20, 100);
    text(`Right-click: Set urgent`, 20, 120);
}

// リセットボタン（Rキー）
function keyPressed() {
    if (key === 'r' || key === 'R') {
        initializeRobots();
    }
}

