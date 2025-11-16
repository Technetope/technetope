// 緊急退避ロジック

class UrgentEscape {
    constructor() {
        this.active = false;
        this.escapeAngle = 0.0;
        this.escapeStartTime = 0;
        this.affectedRobots = new Set();
    }
    
    // 緊急退避が必要かチェック
    checkUrgentEscape(robots, spots) {
        const currentTime = millis();
        let anyUrgent = false;
        this.affectedRobots.clear();
        
        for (const spot of spots) {
            if (!spot.urgent) continue;
            
            for (let i = 0; i < robots.length; i++) {
                const robot = robots[i];
                const dx = robot.x - spot.headX;
                const dy = robot.y - spot.headY;
                const dist = Math.hypot(dx, dy);
                
                // 閾値を広げる（反応を早く、2倍の範囲で反応）
                if (dist < Params.urgent.threshold * 2) {
                    // 緊急退避をアクティブ化
                    this.active = true;
                    this.escapeAngle = this.computeEscapeAngle(robot, spot);
                    this.escapeStartTime = currentTime;
                    this.affectedRobots.add(i);
                    anyUrgent = true;
                }
            }
        }
        
        // 緊急退避の時間制限チェック
        if (this.active) {
            const elapsed = currentTime - this.escapeStartTime;
            if (elapsed > Params.urgent.duration) {
                this.active = false;
                this.affectedRobots.clear();
            }
        }
        
        return anyUrgent;
    }
    
    // 退避方向を計算
    computeEscapeAngle(robot, threat) {
        // ロボットから脅威へのベクトル
        const dx = threat.headX - robot.x;
        const dy = threat.headY - robot.y;
        
        // その逆方向（180度回転）が逃げる方向
        let escapeAngle = Math.atan2(-dy, -dx);
        
        // 脅威の速度も考慮（予測回避）
        const threatSpeed = Math.hypot(threat.velocityX, threat.velocityY);
        if (threatSpeed > Params.urgent.threatSpeedThreshold) {
            // 脅威の進行方向を考慮して、より安全な方向に調整
            const threatHeading = Math.atan2(threat.velocityY, threat.velocityX);
            escapeAngle = threatHeading + Math.PI + 
                         0.3 * (escapeAngle - threatHeading - Math.PI);
        }
        
        return escapeAngle;
    }
    
    // 緊急速度を計算
    computeUrgentVelocity(robotIndex) {
        if (!this.active || !this.affectedRobots.has(robotIndex)) {
            return null;
        }
        
        const vx = Params.urgent.escapeSpeed * Math.cos(this.escapeAngle);
        const vy = Params.urgent.escapeSpeed * Math.sin(this.escapeAngle);
        
        return { vx, vy };
    }
    
    // ロボットが緊急退避中かチェック
    isUrgent(robotIndex) {
        return this.active && this.affectedRobots.has(robotIndex);
    }
}

