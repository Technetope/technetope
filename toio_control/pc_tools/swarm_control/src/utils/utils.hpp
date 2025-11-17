#pragma once

#include <cmath>
#include <random>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace swarm_control {
namespace utils {

// 安全性向上のための定数
constexpr double EPSILON_DISTANCE = 0.001;  // 最小距離（mm）- ゼロ除算防止用
constexpr double MAX_HOMEOSTASIS_ENERGY = 1.5;  // 恒常性エネルギーの上限
constexpr double DEVICE_TIMEOUT_SECONDS = 5.0;  // デバイス切断タイムアウト（秒）

// 距離を計算
inline double distance(double x1, double y1, double x2, double y2) {
    return std::hypot(x2 - x1, y2 - y1);
}

// 角度を正規化（-π から π の範囲に）
inline double normalizeAngle(double angle) {
    while (angle > M_PI) angle -= 2.0 * M_PI;
    while (angle < -M_PI) angle += 2.0 * M_PI;
    return angle;
}

// 角度の差を計算（最短経路）
inline double angleDifference(double angle1, double angle2) {
    double diff = angle2 - angle1;
    return normalizeAngle(diff);
}

// 乱数生成器（各ロボット用に独立したインスタンスを作成可能）
class RandomGenerator {
public:
    RandomGenerator() : gen_(rd_()) {}
    RandomGenerator(unsigned int seed) : gen_(seed) {}
    
    // 0.0 から 1.0 の範囲の乱数
    double random() {
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        return dist(gen_);
    }
    
    // min から max の範囲の乱数
    double random(double min, double max) {
        std::uniform_real_distribution<double> dist(min, max);
        return dist(gen_);
    }
    
    // 整数の乱数（min から max まで、max は含まない）
    int randomInt(int min, int max) {
        std::uniform_int_distribution<int> dist(min, max - 1);
        return dist(gen_);
    }

private:
    std::random_device rd_;
    std::mt19937 gen_;
};

}  // namespace utils
}  // namespace swarm_control

