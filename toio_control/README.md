# Toio Control

Toioロボット群の制御システム（音響制御・群れ行動制御）

## プロジェクト構成

### PC Tools (`pc_tools/`)
- **scheduler/**: 音響タイムライン制御
- **swarm_control/**: Toio群れ制御
- **monitor/**: デバイス監視・ログ集約
- **libs/**: 共有ユーティリティ（OSC、DeviceRegistry）

詳細は [pc_tools/README.md](pc_tools/README.md) を参照。

### Firmware (`firmware/`)
M5StickC Plus2向けファームウェア

### Documentation (`docs/`)
- [アーキテクチャ整理とリファクタリング](docs/architecture_refactoring.md) - **重要**: 最新の整理内容
- [OSC実装](docs/osc_implementation.md)
- [OSC契約仕様](docs/osc_contract.md)

### Archive (`archive/`)
- **development_tools/**: 開発用ツール・実験的実装
  - `swarm_control_p5js/`: p5.jsシミュレーション
  - `unified_app/`: 統合アプリケーション（参考実装）

## クイックスタート

### ビルド
```bash
cmake -S toio_control/pc_tools -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### 実行
```bash
# Swarm Control
./build/swarm_control/swarm_control --config config/swarm_config.json

# Scheduler
./build/scheduler/agent_a_scheduler scheduler/examples/basic_timeline.json
```

## アーキテクチャ

最新のアーキテクチャ整理については、[docs/architecture_refactoring.md](docs/architecture_refactoring.md) を参照してください。

主な変更点:
- 通信レイヤーとアルゴリズムの分離
- 機能別ディレクトリ構造
- 用途別OSC送信実装（scheduler vs swarm_control）

## ライセンス

[LICENSE](LICENSE) を参照してください。
