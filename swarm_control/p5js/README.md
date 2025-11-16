# Toio Swarm Simulation (p5.js)

30台のtoioロボットが人間の動きに反応しながら群れ行動を示すシミュレーション環境。

## 機能

- **スポットベースのポテンシャル場**: 人間の頭位置をスポットとして、その周辺のみポテンシャル場を計算
- **緊急退避**: 人間が急接近した場合の緊急退避モード
- **Boidモデル**: 分離・整列・結合による群れ行動
- **蛇行パターン**: 控えめな蛇行による生命的な動き
- **衝突回避**: ロボット間および境界との衝突回避
- **可視化**: ポテンシャル場、速度ベクトル、スポットの可視化

## 使い方

### サーバーの起動

```bash
# 方法1: 起動スクリプトを使用
./serve.sh

# 方法2: Pythonのhttp.serverを直接使用
python3 -m http.server 8000
```

その後、ブラウザで `http://localhost:8000/toio_swarm_sim.html` を開く

### 操作方法

1. マウスを動かすと人間のスポットが移動
2. 右クリックで緊急退避モードを有効化
3. Rキーでロボットをリセット

## パラメータ調整

`params.js` で各種パラメータを調整可能：

- ポテンシャル強度（`potential.lambda`）
- Boid重み（`boid.separationWeight`, `alignmentWeight`, `cohesionWeight`）
- 蛇行の振幅・周波数（`serpentine.amplitude`, `frequency`）
- 緊急退避の閾値（`urgent.threshold`）
- 更新頻度（`potential.updateInterval`）

## ファイル構成

- `toio_swarm_sim.html`: メインHTMLファイル
- `sketch.js`: p5.jsのメインスケッチ
- `params.js`: パラメータ設定
- `spot_potential.js`: スポットベースポテンシャル場
- `urgent_escape.js`: 緊急退避ロジック
- `boid_model.js`: Boidモデル実装
- `robot_agent.js`: 個別ロボットエージェント

## 数式

詳細な数式は計画書（`p5-js-toio-swarm-simulation.plan.md`）を参照。

## 実機への展開

このシミュレーションで最適化したパラメータを `swarm/cpp_client/samples/motion_planner.cpp` に反映して実機制御を行う。

