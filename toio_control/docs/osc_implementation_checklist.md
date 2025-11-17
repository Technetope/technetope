# OSC実装確認チェックリスト

このドキュメントは、OSC通信実装が正しく動作しているかを確認するためのチェックリストです。

## 実装ファイルの場所

### 基盤ライブラリ

- [ ] `toio_control/pc_tools/libs/include/toio_control/osc/OscTransport.h` - 送受信クラス定義
- [ ] `toio_control/pc_tools/libs/include/toio_control/osc/OscPacket.h` - パケット構造体定義
- [ ] `toio_control/pc_tools/libs/include/toio_control/osc/OscEncryptor.h` - 暗号化クラス定義
- [ ] `toio_control/pc_tools/libs/src/OscTransport.cpp` - 送受信実装
- [ ] `toio_control/pc_tools/libs/src/OscPacket.cpp` - エンコード/デコード実装
- [ ] `toio_control/pc_tools/libs/src/OscEncryptor.cpp` - 暗号化実装

### 音響制御（Scheduler）

- [ ] `toio_control/pc_tools/scheduler/src/SchedulerController.cpp` - OSC送信実装（157-187行目）
- [ ] `toio_control/pc_tools/scheduler/include/toio_control/scheduler/SchedulerController.h` - クラス定義
- [ ] `toio_control/firmware/src/modules/OscReceiver.cpp` - ファームウェア側受信実装
- [ ] `toio_control/firmware/src/modules/OscReceiver.h` - ファームウェア側受信クラス定義

### Toio制御（Swarm Control）

- [ ] `toio_control/pc_tools/swarm_control/src/comm/osc_sender.cpp` - OSC送信実装
- [ ] `toio_control/pc_tools/swarm_control/src/comm/osc_sender.hpp` - 送信クラス定義
- [ ] `toio_control/pc_tools/swarm_control/src/comm/osc_receiver.cpp` - OSC受信実装
- [ ] `toio_control/pc_tools/swarm_control/src/comm/osc_receiver.hpp` - 受信クラス定義

### 統合アプリケーション

- [ ] `toio_control/pc_tools/unified/src/main.cpp` - 統合実装（83-160行目）
- [ ] `toio_control/pc_tools/unified/src/unified_config.cpp` - 設定管理

### プロトコル仕様書

- [ ] `toio_control/docs/osc_contract.md` - 音響制御プロトコル仕様
- [ ] `m5-toio/firmware/src/protocole.md` - Toio制御プロトコル仕様
- [ ] `toio_control/pc_tools/swarm_control/README.md` - Swarm Controlプロトコル仕様

---

## 実装確認項目

### 1. 基盤ライブラリの確認

#### OscTransport

- [ ] `IoContextRunner`が正しくI/Oコンテキストを管理しているか
- [ ] `OscSender`がメッセージ/バンドルを正しく送信できるか
- [ ] `OscListener`がパケットを正しく受信できるか
- [ ] ブロードキャスト設定が正しく動作するか
- [ ] エンドポイントの変更が反映されるか

#### OscPacket

- [ ] メッセージのエンコード/デコードが正しく動作するか
- [ ] バンドルのエンコード/デコードが正しく動作するか
- [ ] 各種引数型（int32, float, string, bool, blob）が正しく処理されるか
- [ ] Timetagの変換が正しく動作するか

#### OscEncryptor

- [ ] AES-256-CTR暗号化が正しく動作するか
- [ ] カウンタベースのIV導出が正しく動作するか
- [ ] 暗号化/復号が対称的に動作するか
- [ ] カウンタのオーバーフロー処理が正しいか

### 2. 音響制御の確認

#### Scheduler（PC側）

- [ ] タイムラインJSONからOSCバンドルが正しく生成されるか
- [ ] Timetagが正しく設定されるか（NTP時刻）
- [ ] 暗号化が有効な場合、正しく暗号化されるか
- [ ] バンドル間隔が正しく制御されるか
- [ ] エラーハンドリングが適切か

#### Firmware（M5側）

- [ ] OSCパケットが正しく受信できるか
- [ ] 暗号化パケットが正しく復号できるか
- [ ] `/acoustics/play`メッセージが正しく処理されるか
- [ ] `/acoustics/stop`メッセージが正しく処理されるか
- [ ] Timetagに基づく再生スケジューリングが正しく動作するか
- [ ] 未知のプリセットIDが適切に無視されるか

### 3. Toio制御の確認

#### Swarm Control（PC側）

**送信側**:
- [ ] `/toio/{robotIndex}/target`メッセージが正しく送信されるか
- [ ] 複数ロボットの目標座標がバンドル形式で送信されるか
- [ ] 1秒間隔で送信されるか
- [ ] 暗号化が有効な場合、正しく暗号化されるか

**受信側**:
- [ ] `/announce`メッセージが正しく処理されるか
- [ ] `/heartbeat`メッセージが正しく処理されるか
- [ ] `/toio/position`メッセージが正しく処理されるか
- [ ] デバイス登録が正しく動作するか
- [ ] 座標データが正しく取得されるか

#### M5+Toio中継機

- [ ] `/toio/scan`コマンドが正しく処理されるか
- [ ] `/toio/connect`コマンドが正しく処理されるか
- [ ] `/toio/led`コマンドが正しく処理されるか
- [ ] `/toio/motor`コマンドが正しく処理されるか
- [ ] `/toio/goal-set`コマンドが正しく処理されるか
- [ ] `/toio/status`通知が正しく送信されるか

### 4. 統合アプリケーションの確認

- [ ] 統一OSC送受信器が正しく初期化されるか
- [ ] Swarm ControlとSchedulerが同じOSC送受信器を使用しているか
- [ ] 設定ファイルから正しく設定が読み込まれるか
- [ ] 暗号化設定が正しく反映されるか
- [ ] エラーハンドリングが適切か

---

## プロトコル仕様の確認

### 音響制御プロトコル

- [ ] `/acoustics/play`の引数形式が仕様通りか
- [ ] `/acoustics/stop`の引数形式が仕様通りか
- [ ] `/announce`の引数形式が仕様通りか
- [ ] `/heartbeat`の引数形式が仕様通りか
- [ ] Timetagの処理が仕様通りか
- [ ] 暗号化方式が仕様通りか（AES-CTR、カウンタ8バイト）

**参照**: `toio_control/docs/osc_contract.md`

### Toio制御プロトコル

- [ ] `/toio/scan`の引数形式が仕様通りか
- [ ] `/toio/connect`の引数形式が仕様通りか
- [ ] `/toio/led`の引数形式が仕様通りか
- [ ] `/toio/motor`の引数形式が仕様通りか
- [ ] `/toio/goal-set`の引数形式が仕様通りか
- [ ] `/toio/status`の引数形式が仕様通りか

**参照**: `m5-toio/firmware/src/protocole.md`

### Swarm Controlプロトコル

- [ ] `/toio/{robotIndex}/target`の引数形式が仕様通りか
- [ ] `/toio/position`の引数形式が仕様通りか
- [ ] バンドル形式での送信が仕様通りか

**参照**: `toio_control/pc_tools/swarm_control/README.md`

---

## テスト方法

### 1. 単体テスト

```bash
# Schedulerのテスト
cd build/toio_control
./scheduler/toio_control_scheduler_tests

# OscEncryptorのテスト
./scheduler/toio_control_scheduler_tests -v [osc]
```

### 2. 統合テスト

```bash
# 統合アプリケーションの実行
./unified/toio_control -c config/toio_control_config.json
```

### 3. 実機テスト

1. M5StickC Plus2を起動
2. `/announce`メッセージが受信されることを確認
3. `/toio/position`メッセージが受信されることを確認
4. `/toio/{robotIndex}/target`メッセージが送信されることを確認
5. Toioロボットが正しく動作することを確認

---

## よくある問題と対処法

### 暗号化エラー

**症状**: パケットが復号できない

**確認項目**:
- [ ] `osc_config.json`のキー/IVがPC側とM5側で一致しているか
- [ ] カウンタが正しくインクリメントされているか
- [ ] IV導出ロジックが正しいか

**対処法**:
- キー/IVを再生成
- カウンタをリセット
- `OscEncryptor.cpp`の実装を確認

### メッセージが届かない

**症状**: 送信したメッセージが受信されない

**確認項目**:
- [ ] ポート番号が正しいか
- [ ] IPアドレスが正しいか
- [ ] ブロードキャスト設定が有効か
- [ ] ファイアウォールがブロックしていないか

**対処法**:
- ネットワーク設定を確認
- `tcpdump`や`wireshark`でパケットを確認
- ファイアウォール設定を確認

### タイムライン同期の問題

**症状**: 音響再生のタイミングがずれる

**確認項目**:
- [ ] NTP同期が正しく動作しているか
- [ ] Timetagが正しく設定されているか
- [ ] システム時刻が正しいか

**対処法**:
- NTPサーバーの設定を確認
- システム時刻を同期
- Timetagの処理ロジックを確認

---

## 実装の完全性チェック

### 必須機能

- [ ] OSCメッセージの送信
- [ ] OSCメッセージの受信
- [ ] OSCバンドルの送信
- [ ] OSCバンドルの受信
- [ ] 暗号化（AES-256-CTR）
- [ ] 復号
- [ ] エラーハンドリング
- [ ] ログ出力

### 音響制御機能

- [ ] タイムラインからのバンドル生成
- [ ] Timetagの設定
- [ ] プリセットIDの検証
- [ ] 再生キューへの追加

### Toio制御機能

- [ ] デバイス登録
- [ ] 座標受信
- [ ] 目標座標送信
- [ ] ハートビート処理

---

## ドキュメントの確認

- [ ] `osc_implementation.md`が最新か
- [ ] `osc_contract.md`が最新か
- [ ] `protocole.md`が最新か
- [ ] READMEファイルが最新か
- [ ] コードコメントが適切か

---

## まとめ

このチェックリストを使用して、OSC通信実装が正しく動作していることを確認してください。

問題が見つかった場合は、`toio_control/docs/troubleshooting.md`も参照してください。

