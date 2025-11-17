# PC Tools Module

PCツール群は C++17 + CMake を想定した再構築中であり、以下のサブモジュールに分割されています。

## モジュール構成

### 1. scheduler/ - 音響タイムライン制御
- **用途**: タイムライン編集、OSCバンドル生成、コマンド送出
- **依存**: `CLI11`, `nlohmann/json`, `asio`, `fmt`, `spdlog`
- **構造**: 
  - `audio/`: 音響タイムライン関連
  - `osc/`: OSC送信関連（低頻度バッチ送信に最適化）
  - `config/`: 設定・ターゲット解決関連
- **詳細**: [scheduler/README.md](scheduler/README.md)

### 2. swarm_control/ - Toio群れ制御
- **用途**: Toioロボット群のリアルタイム制御（群れ行動、衝突回避、人間回避など）
- **依存**: `asio`, `spdlog`, `fmt`
- **構造**:
  - `algorithm/`: アルゴリズム・モデル（agent, collision, flocking, spatial）
  - `comm/`: 通信レイヤー（OSC送受信、高頻度送信に最適化）
  - `config/`: 設定
  - `utils/`: ユーティリティ（device, types, utils）
- **詳細**: [swarm_control/README.md](swarm_control/README.md)

### 3. monitor/ - 遅延計測・心拍受信・ログ集約
- **用途**: デバイス監視、心拍受信、ログ集約
- **依存**: `asio`, `spdlog`
- **機能**: `DeviceRegistry` が `/announce` を永続化し、CSV ログを追記

### 4. libs/ - 共有ユーティリティ
- **内容**: OSC パケット・トランスポート、DeviceRegistry、暗号フック
- **用途**: 全モジュールで共有される共通ライブラリ

### 5. unified/ - 統合アプリケーション
- **用途**: Toioの動きとacousticsの制御を統一的に行う統合アプリケーション
- **機能**: 
  - Swarm ControlとSchedulerを一つのビルドで統合
  - 一つのプロセス内で両機能を並行実行
  - 用途に応じて最適化された別々のOSC送信実装を使用
- **詳細**: [unified/README.md](unified/README.md)、[docs/unified_runbook.md](../docs/unified_runbook.md)

## OSC送信実装の違い

### scheduler (音響制御)
- **クラス**: `toio_control::scheduler::osc::OscBundleSender`
- **用途**: 低頻度のバッチ送信（タイムラインに基づく）
- **特徴**: 送信完了後に切断可能（リソース効率化）

### swarm_control (toio制御)
- **クラス**: `swarm_control::OscSender`
- **用途**: 高頻度の連続送信（1秒ごとなど）
- **特徴**: 常時接続を維持（レイテンシ最小化）

詳細は [architecture_refactoring.md](../docs/architecture_refactoring.md) を参照してください。

## セットアップ手順

```bash
# ビルドディレクトリを作成
cmake -S toio_control/pc_tools -B build -DCMAKE_BUILD_TYPE=Release

# ビルド
cmake --build build

# テスト実行（実装後）
ctest --test-dir build
```

## 実行例

### Scheduler
  ```bash
./build/scheduler/agent_a_scheduler \
  scheduler/examples/basic_timeline.json \
  --host 192.168.10.255 \
  --port 9000 \
  --bundle-spacing 0.02 \
  --target-map mappings/voices.json \
  --default-targets dev-001,dev-002,dev-003 \
  --osc-config secrets/osc_config.json
```

### Swarm Control
  ```bash
./build/swarm_control/swarm_control \
  --config config/swarm_config.json
  ```

### Unified (統合アプリケーション)
  ```bash
# Toioの動きとacousticsの制御を統一的に行う
./build/unified/toio_control \
  -c config/toio_control_config.json
  ```

設定ファイルで`swarm.enabled`と`scheduler.enabled`を制御できます。

## アーキテクチャ

詳細なアーキテクチャとリファクタリングの内容については、[docs/architecture_refactoring.md](../docs/architecture_refactoring.md) を参照してください。
