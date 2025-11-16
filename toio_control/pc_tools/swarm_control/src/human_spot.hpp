#pragma once

namespace swarm_control {

struct HumanSpot {
    double headX = 0.0;        // 頭の位置 X (mm)
    double headY = 0.0;        // 頭の位置 Y (mm)
    double velocityX = 0.0;    // 速度 X (mm/s)
    double velocityY = 0.0;    // 速度 Y (mm/s)
    bool urgent = false;       // 緊急フラグ
};

}  // namespace swarm_control

