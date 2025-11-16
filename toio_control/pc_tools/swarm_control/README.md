# Swarm Control (C++)

Toioロボット群の制御システム（C++実装版）

## 概要

p5.jsで実装されたシミュレーションをC++に移植し、実機のToioロボットを制御するシステムです。
toio_controlとの統合を考慮した設計となっています。

## 主な機能

- **動的アサイン**: 接続順に基づいてロボットに速度カテゴリを割り当て（20% FAST、60% MODERATE、20% SLOW）
- **DeviceRegistry統合**: toio_controlの`DeviceRegistry`を使用してデバイス管理
- **OSC通信**: 座標データの送受信（暗号化サポート）
- **永続化**: アサイン情報をJSON形式で保存
- **エラーハンドリング**: デバイス未登録、重複登録、ストレージエラーに対応

## ビルド方法

```bash
cd /home/ksk432/biotope/swarm_control
mkdir -p build && cd build
cmake ..
make -j4
```

## 実行方法

```bash
./swarm_control
```

## 設定ファイル

`config/swarm_config.json`を作成して設定をカスタマイズできます：

```json
{
  "osc": {
    "listen_port": 5006,
    "send_port": 5005,
    "send_address": "127.0.0.1",
    "encryption": {
      "enabled": false,
      "key_file": "osc_config.json"
    }
  },
  "swarm": {
    "assignment_storage": "state/swarm_assignments.json",
    "device_registry": "state/devices.json"
  }
}
```

設定ファイルが存在しない場合は、デフォルト値が使用されます。

## データフロー

1. **デバイス起動**: M5デバイスが起動し、`/announce`メッセージを送信
2. **デバイス登録**: `DeviceRegistry`にデバイスを登録（MACアドレスからデバイスIDを生成）
3. **座標受信**: `/toio/position`メッセージで座標を受信
4. **動的アサイン**: 接続順に基づいてカテゴリを割り当て
5. **ロボット作成**: `RobotAgent`を作成してシミュレーションに参加
6. **制御ループ**: 1秒ごとにロボットの状態を更新し、OSCで目標座標を送信

## ディレクトリ構造

```
swarm_control/
├── src/
│   ├── main.cpp              # メインループ
│   ├── device_manager.hpp/cpp # デバイス管理（DeviceRegistry統合）
│   ├── osc_receiver.hpp/cpp   # OSC受信（/announce, /heartbeat, /toio/position）
│   ├── osc_sender.hpp/cpp     # OSC送信
│   ├── config.hpp/cpp         # 設定ファイル管理
│   └── ...                    # その他のコンポーネント
├── config/
│   └── swarm_config.json      # 設定ファイル（オプション）
├── state/
│   ├── devices.json           # デバイスレジストリ（DeviceRegistryが管理）
│   └── swarm_assignments.json # アサイン情報（DeviceManagerが管理）
└── CMakeLists.txt
```

## OSCメッセージ仕様

### 受信メッセージ

- `/announce`: デバイス登録
  - 引数: `[deviceId (string), mac (string), firmwareVersion (string)]`
- `/heartbeat`: ハートビート（オプション）
  - 引数: `[deviceId (string), sequence (int32), seconds (int32), micros (int32), ...]`
- `/toio/position`: 座標データ
  - 引数: `[x (float), y (float), angle (float), deviceId (string)]` または
  - アドレス: `/toio/{deviceId}/position`, 引数: `[x (float), y (float), angle (float)]`

### 送信メッセージ

- `/toio/{robotIndex}/target`: 目標座標
  - 引数: `[x (float), y (float), angle (float)]`
  - 1秒ごとにバンドル形式で一括送信

## 動的アサイン

接続順に基づいて、現在の接続数に応じて割合的にカテゴリを割り当てます：

- **10個接続時**: 最初の2個（20%）がFAST、次の6個（60%）がMODERATE、最後の2個（20%）がSLOW
- **15個接続時**: 最初の3個（20%）がFAST、次の9個（60%）がMODERATE、最後の3個（20%）がSLOW

## 依存関係

- `toio_control_osc`: OSC通信ライブラリ
- `asio`: 非同期I/O
- `spdlog`: ログ出力
- `fmt`: 文字列フォーマット
- `OpenSSL`: 暗号化（将来の拡張用）
- `nlohmann/json`: JSON処理

## 注意事項

- デバイスは`/announce`メッセージで登録後、`/toio/position`メッセージでアサインされます
- アサイン情報は`state/swarm_assignments.json`に保存されます
- デバイスが5秒間更新されない場合、自動的に切断とみなされます
