# OSC実装 クイックリファレンス

OSC通信実装の構成を素早く確認するためのリファレンスです。

## 📁 実装ファイルの場所

### 基盤ライブラリ（共通実装）

| ファイル | 説明 |
|---------|------|
| `pc_tools/libs/include/toio_control/osc/OscTransport.h` | 送受信クラス定義 |
| `pc_tools/libs/include/toio_control/osc/OscPacket.h` | パケット構造体定義 |
| `pc_tools/libs/include/toio_control/osc/OscEncryptor.h` | 暗号化クラス定義 |
| `pc_tools/libs/src/OscTransport.cpp` | 送受信実装 |
| `pc_tools/libs/src/OscPacket.cpp` | エンコード/デコード実装 |
| `pc_tools/libs/src/OscEncryptor.cpp` | 暗号化実装 |

### 音響制御（Scheduler）

| ファイル | 説明 |
|---------|------|
| `pc_tools/scheduler/src/SchedulerController.cpp` | OSC送信（157-187行目） |
| `firmware/src/modules/OscReceiver.cpp` | ファームウェア側受信 |

### Toio制御（Swarm Control）

| ファイル | 説明 |
|---------|------|
| `pc_tools/swarm_control/src/comm/osc_sender.cpp` | OSC送信 |
| `pc_tools/swarm_control/src/comm/osc_sender.hpp` | 送信クラス定義 |
| `pc_tools/swarm_control/src/comm/osc_receiver.cpp` | OSC受信 |
| `pc_tools/swarm_control/src/comm/osc_receiver.hpp` | 受信クラス定義 |

### 統合アプリケーション

| ファイル | 説明 |
|---------|------|
| `pc_tools/unified/src/main.cpp` | 統合実装（83-160行目） |

## 📋 プロトコル仕様書

| ファイル | 説明 |
|---------|------|
| `docs/osc_contract.md` | 音響制御プロトコル仕様 |
| `m5-toio/firmware/src/protocole.md` | Toio制御プロトコル仕様 |
| `pc_tools/swarm_control/README.md` | Swarm Controlプロトコル仕様 |

## 🔍 実装確認のポイント

### 1. 基盤ライブラリ
→ `pc_tools/libs/include/toio_control/osc/` と `pc_tools/libs/src/`

### 2. 音響制御
→ `pc_tools/scheduler/src/SchedulerController.cpp` (157-187行目)
→ `firmware/src/modules/OscReceiver.cpp`

### 3. Toio制御
→ `pc_tools/swarm_control/src/comm/`

### 4. 統合アプリケーション
→ `pc_tools/unified/src/main.cpp` (83-160行目)

## 📚 詳細ドキュメント

- **実装全体像**: `docs/osc_implementation.md`
- **確認チェックリスト**: `docs/osc_implementation_checklist.md`
- **プロトコル仕様**: `docs/osc_contract.md`

## 🚀 クイックスタート

### 実装を確認する場合

1. **基盤ライブラリ**: `pc_tools/libs/include/toio_control/osc/`
2. **使用箇所**: `pc_tools/scheduler/`, `pc_tools/swarm_control/src/comm/`
3. **プロトコル**: `docs/osc_contract.md`, `m5-toio/firmware/src/protocole.md`

### 問題を調査する場合

1. **実装確認**: `docs/osc_implementation.md`
2. **チェックリスト**: `docs/osc_implementation_checklist.md`
3. **トラブルシューティング**: `docs/troubleshooting.md`

