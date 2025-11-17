# Toio Control - Unified Application

Swarm robot制御とサウンドコントロールを統合した単一アプリケーションです。

## 概要

このアプリケーションは、以下の機能を統合しています：

- **Swarm Control**: Toioロボット群の制御（群れ行動、衝突回避、人間回避など）
- **Sound Scheduler**: タイムラインに基づくサウンド再生制御

## ビルド方法

```bash
cd /home/ksk432/biotope
cmake -S toio_control/pc_tools -B build/toio_control -DCMAKE_BUILD_TYPE=Release
cmake --build build/toio_control --target toio_control
```

## 実行方法

### 最小限の引数（設定ファイル使用）

```bash
./build/toio_control/unified/toio_control
```

デフォルトで `config/toio_control_config.json` を読み込みます。

### カスタム設定ファイルを指定

```bash
./build/toio_control/unified/toio_control -c /path/to/config.json
```

## 設定ファイル

設定ファイルのサンプル: `config/toio_control_config.example.json`

### 設定項目

#### OSC設定
- `osc.listen_port`: OSC受信ポート（デフォルト: 5006）
- `osc.send_port`: OSC送信ポート（デフォルト: 5005）
- `osc.send_address`: OSC送信先アドレス（デフォルト: 255.255.255.255）
- `osc.broadcast`: ブロードキャスト有効化（デフォルト: true）
- `osc.encryption.enabled`: 暗号化有効化（デフォルト: false）
- `osc.encryption.key_file`: 暗号化キーファイルパス

#### Swarm設定
- `swarm.enabled`: Swarm制御を有効化（デフォルト: true）
- `swarm.assignment_storage`: アサイン情報保存先（デフォルト: state/swarm_assignments.json）
- `swarm.device_registry`: デバイスレジストリパス（デフォルト: state/devices.json）

#### Scheduler設定
- `scheduler.enabled`: Schedulerを有効化（デフォルト: false）
- `scheduler.timeline_path`: タイムラインJSONファイルパス
- `scheduler.lead_time`: リードタイム（秒、デフォルト: 3.0）
- `scheduler.bundle_spacing`: バンドル送信間隔（秒、デフォルト: 0.01）
- `scheduler.target_map`: ターゲットマッピングファイル（オプション）
- `scheduler.default_targets`: デフォルトターゲット（配列またはカンマ区切り文字列）

## 機能

### Swarm Control

- デバイス自動登録（`/announce`メッセージ）
- 座標受信（`/toio/position`メッセージ）
- 動的アサイン（接続順に基づく速度カテゴリ割り当て）
- 群れ行動シミュレーション
- 目標座標送信（`/toio/{robotIndex}/target`）

### Sound Scheduler

- タイムラインJSONファイルから再生スケジュールを読み込み
- OSCメッセージ（`/acoustics/play`など）を送信
- 暗号化サポート

## 実機テスト

実機テストを想定した設計となっており、設定ファイルで動作を制御できます。

1. 設定ファイルを作成（`config/toio_control_config.json`）
2. 必要に応じてOSC暗号化キーを設定
3. アプリケーションを起動

```bash
./build/toio_control/unified/toio_control
```

## 注意事項

- Swarm ControlとSchedulerは同じOSC送信器を共有します
- Schedulerは別スレッドで実行されます
- 設定ファイルが存在しない場合はデフォルト値が使用されます

