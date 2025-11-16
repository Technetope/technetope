#include "params.hpp"

namespace swarm_control {
namespace params {

AllParams g_params;

void initializeParams() {
    // デフォルト値は構造体の初期化子で設定済み
    // 将来的に設定ファイルから読み込む場合はここで実装
}

}  // namespace params
}  // namespace swarm_control

