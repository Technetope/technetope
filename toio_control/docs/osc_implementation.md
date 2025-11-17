# OSC通信実装の全体像

このドキュメントは、Toio ControlシステムにおけるOSC（Open Sound Control）通信の実装構成とプロトコル仕様を説明します。

## 目次

1. [実装構成](#実装構成)
2. [OSCライブラリ（基盤実装）](#oscライブラリ基盤実装)
3. [使用箇所](#使用箇所)
4. [プロトコル仕様](#プロトコル仕様)
5. [実装確認のポイント](#実装確認のポイント)

---

## 実装構成

### ディレクトリ構造

```
toio_control/
├── pc_tools/
│   ├── libs/                          # OSC基盤ライブラリ
│   │   ├── include/toio_control/osc/
│   │   │   ├── OscTransport.h        # 送受信クラス
│   │   │   ├── OscPacket.h           # パケットエンコード/デコード
│   │   │   └── OscEncryptor.h        # AES-CTR暗号化
│   │   └── src/
│   │       ├── OscTransport.cpp
│   │       ├── OscPacket.cpp
│   │       └── OscEncryptor.cpp
│   ├── scheduler/                     # 音響制御（OSC送信）
│   ├── swarm_control/                 # Toio制御（OSC送受信）
│   │   └── src/comm/
│   │       ├── osc_sender.cpp/hpp     # OSC送信
│   │       └── osc_receiver.cpp/hpp   # OSC受信
│   ├── monitor/                       # 心拍監視（OSC受信）
│   └── unified/                       # 統合アプリケーション
├── firmware/                          # M5StickC Plus2 ファームウェア
│   └── src/modules/
│       └── OscReceiver.cpp            # 音響制御のOSC受信
└── m5-toio/                           # M5+Toio中継機
    └── firmware/src/
        └── protocole.md               # Toio制御プロトコル仕様
```

---

## OSCライブラリ（基盤実装）

### 場所

**`toio_control/pc_tools/libs/include/toio_control/osc/`**

### 主要クラス

#### 1. `OscTransport.h` - 送受信基盤

**ファイル**: `toio_control/pc_tools/libs/include/toio_control/osc/OscTransport.h`

**主要クラス**:

- **`IoContextRunner`**: ASIO I/Oコンテキストの管理
  - バックグラウンドスレッドでI/O処理を実行
  - 複数のOSC送受信器で共有可能

- **`OscSender`**: OSCメッセージ/バンドルの送信
  ```cpp
  OscSender(asio::io_context& ctx, const Endpoint& destination, bool allowBroadcast);
  void send(const Message& message);
  void send(const Bundle& bundle);
  void enableEncryption(const Key256& key, const Iv128& iv);
  ```

- **`OscListener`**: OSCパケットの受信
  ```cpp
  OscListener(asio::io_context& ctx, const Endpoint& listenEndpoint, PacketHandler handler);
  void start();
  void enableEncryption(const Key256& key, const Iv128& iv);
  ```

**実装確認**: `toio_control/pc_tools/libs/src/OscTransport.cpp`

#### 2. `OscPacket.h` - パケット処理

**ファイル**: `toio_control/pc_tools/libs/include/toio_control/osc/OscPacket.h`

**主要機能**:

- **`Message`**: OSCメッセージ構造体
  ```cpp
  struct Message {
      std::string address;
      std::vector<Argument> arguments;  // int32, float, string, bool, blob
  };
  ```

- **`Bundle`**: OSCバンドル構造体（複数メッセージをまとめる）
  ```cpp
  struct Bundle {
      std::uint64_t timetag;  // NTP 64-bit
      std::vector<Message> elements;
  };
  ```

- **エンコード/デコード関数**:
  ```cpp
  std::vector<uint8_t> encodeMessage(const Message& message);
  std::vector<uint8_t> encodeBundle(const Bundle& bundle);
  Packet decodePacket(const std::vector<uint8_t>& payload);
  ```

**実装確認**: `toio_control/pc_tools/libs/src/OscPacket.cpp`

#### 3. `OscEncryptor.h` - 暗号化

**ファイル**: `toio_control/pc_tools/libs/include/toio_control/osc/OscEncryptor.h`

**機能**:

- **AES-256-CTR暗号化**: PC → M5の通信を暗号化
- **カウンタベースIV**: 送信ごとにカウンタをインクリメントし、IVを導出
- **OpenSSL使用**: `EVP_aes_256_ctr()`で実装

**実装確認**: `toio_control/pc_tools/libs/src/OscEncryptor.cpp`

**暗号化方式**:
- 平文先頭に8バイトのカウンタ（Big Endian）を付与
- `IV + counter`でAES-CTR暗号化
- カウンタは送信ごとにインクリメント

---

## 使用箇所

### 1. Scheduler（音響制御 - OSC送信）

**場所**: `toio_control/pc_tools/scheduler/src/SchedulerController.cpp`

**機能**: タイムラインJSONからOSCバンドルを生成して送信

**使用例**:
```cpp
// sendBundles()関数内（157-187行目）
toio_control::osc::OscSender sender(ioContext, endpoint, config.broadcast);
if (config.encryptOsc) {
    sender.enableEncryption(*config.oscKey, *config.oscIv);
}
sender.send(bundles[i].toOscBundle());
```

**送信メッセージ**:
- `/acoustics/play`: 音響プリセット再生
- `/acoustics/stop`: 音響停止

**プロトコル仕様**: `toio_control/docs/osc_contract.md`

### 2. Swarm Control（Toio制御 - OSC送受信）

**場所**: `toio_control/pc_tools/swarm_control/src/`

#### 送信側: `osc_sender.cpp`

**機能**: Toioロボットの目標座標を送信

**使用例**:
```cpp
OscSender oscSender(config.osc.sendAddress, config.osc.sendPort);
oscSender.sendTargets(targets);  // 複数ロボットの目標座標を一括送信
```

**送信メッセージ**:
- `/toio/{robotIndex}/target`: 目標座標 `[x (float), y (float), angle (float)]`

#### 受信側: `osc_receiver.cpp`

**機能**: Toioロボットからの座標情報を受信

**使用例**:
```cpp
OscReceiver oscReceiver(config.osc.listenPort);
oscReceiver.setPositionCallback([&](const ReceivedPosition& pos) {
    // 座標処理
});
oscReceiver.start();
```

**受信メッセージ**:
- `/announce`: デバイス登録 `[deviceId, mac, firmwareVersion]`
- `/heartbeat`: ハートビート `[deviceId, sequence, seconds, micros, ...]`
- `/toio/position`: 座標データ `[x, y, angle, deviceId]` または `/toio/{deviceId}/position` `[x, y, angle]`

**実装確認**:
- `swarm_control/src/comm/osc_receiver.cpp`: メッセージ処理ロジック
- `swarm_control/src/comm/osc_receiver.hpp`: コールバック定義
- `swarm_control/src/comm/osc_sender.cpp`: 送信実装
- `swarm_control/src/comm/osc_sender.hpp`: 送信クラス定義

### 3. Monitor（心拍監視 - OSC受信）

**場所**: `toio_control/pc_tools/monitor/src/main.cpp`

**機能**: デバイスからの心拍メッセージを受信・記録

**受信メッセージ**:
- `/announce`: デバイス登録
- `/heartbeat`: 心拍情報

**実装確認**: `toio_control/pc_tools/monitor/src/main.cpp`

### 4. Unified Application（統合アプリケーション）

**場所**: `toio_control/pc_tools/unified/src/main.cpp`

**機能**: Swarm ControlとSchedulerを統合し、統一OSC送受信器を使用

**実装確認**: `toio_control/pc_tools/unified/src/main.cpp` (83-160行目)

---

## プロトコル仕様

### 音響制御プロトコル

**仕様書**: `toio_control/docs/osc_contract.md`

**主要メッセージ**:

#### PC → M5StickC

- **`/acoustics/play`**: 音響プリセット再生
  - 引数: `[presetId (string), time (optional), gain (optional), loop (optional)]`
  
- **`/acoustics/stop`**: 音響停止
  - 引数: `[time (optional)]`

#### M5StickC → PC

- **`/announce`**: デバイス登録
  - 引数: `[deviceId (string), mac (string), firmwareVersion (string)]`

- **`/heartbeat`**: 心拍情報
  - 引数: `[deviceId, sequence, seconds, micros, queueSize, playing]`

**詳細**: `toio_control/docs/osc_contract.md` を参照

### Toio制御プロトコル

**仕様書**: `m5-toio/firmware/src/protocole.md`

**主要メッセージ**:

#### PC → M5 (Toio中継機)

- **`/toio/scan`**: スキャン実行
- **`/toio/connect <suffix>`**: Toio接続
- **`/toio/led <r> <g> <b>`**: LED制御
- **`/toio/motor <left> <right>`**: モータ制御
- **`/toio/goal-set <x> <y> <stop_distance>`**: ゴール設定
- **`/toio/goal-clear`**: ゴールクリア
- **`/toio/status-request`**: 状態要求
- **`/toio/status-subscribe <enable>`**: 状態購読

#### M5 (Toio中継機) → PC

- **`/toio/scan-result`**: スキャン結果
- **`/toio/connect-result`**: 接続結果
- **`/toio/status`**: 状態通知
- **`/toio/goal-reached`**: ゴール到達
- **`/toio/error`**: エラー通知

**詳細**: `m5-toio/firmware/src/protocole.md` を参照

### Swarm Controlプロトコル

**仕様書**: `toio_control/pc_tools/swarm_control/README.md` (85-101行目)

**主要メッセージ**:

#### PC → M5 (Swarm制御)

- **`/toio/{robotIndex}/target`**: 目標座標
  - 引数: `[x (float), y (float), angle (float)]`
  - 1秒ごとにバンドル形式で一括送信

#### M5 → PC (Swarm制御)

- **`/toio/position`**: 座標データ
  - 引数: `[x (float), y (float), angle (float), deviceId (string)]`
  - または: `/toio/{deviceId}/position` `[x, y, angle]`

---

## 実装確認のポイント

### 1. OSC基盤ライブラリの確認

**確認箇所**:
- `toio_control/pc_tools/libs/include/toio_control/osc/OscTransport.h`: クラス定義
- `toio_control/pc_tools/libs/src/OscTransport.cpp`: 実装詳細
- `toio_control/pc_tools/libs/src/OscPacket.cpp`: エンコード/デコード実装
- `toio_control/pc_tools/libs/src/OscEncryptor.cpp`: 暗号化実装

**確認ポイント**:
- ✅ 送受信の非同期処理（ASIO使用）
- ✅ 暗号化の実装（AES-256-CTR）
- ✅ バンドル/メッセージのエンコード/デコード
- ✅ エラーハンドリング

### 2. 音響制御の実装確認

**確認箇所**:
- `toio_control/pc_tools/scheduler/src/SchedulerController.cpp` (157-187行目): OSC送信
- `toio_control/firmware/src/modules/OscReceiver.cpp`: ファームウェア側受信
- `toio_control/docs/osc_contract.md`: プロトコル仕様

**確認ポイント**:
- ✅ タイムラインからOSCバンドル生成
- ✅ Timetagの処理（NTP時刻）
- ✅ 暗号化送信
- ✅ ファームウェア側の復号・処理

### 3. Toio制御の実装確認

**確認箇所**:
- `toio_control/pc_tools/swarm_control/src/osc_sender.cpp`: 送信実装
- `toio_control/pc_tools/swarm_control/src/osc_receiver.cpp`: 受信実装
- `m5-toio/firmware/src/protocole.md`: プロトコル仕様

**確認ポイント**:
- ✅ 目標座標の送信（1秒間隔）
- ✅ 座標データの受信・処理
- ✅ デバイス登録（`/announce`）
- ✅ ハートビート処理

### 4. 統合アプリケーションの確認

**確認箇所**:
- `toio_control/pc_tools/unified/src/main.cpp`: 統合実装
- `toio_control/pc_tools/unified/src/unified_config.cpp`: 設定管理

**確認ポイント**:
- ✅ 統一OSC送受信器の使用
- ✅ Swarm ControlとSchedulerの統合
- ✅ 設定ファイルベースの動作

### 5. プロトコル仕様の確認

**確認箇所**:
- `toio_control/docs/osc_contract.md`: 音響制御プロトコル
- `m5-toio/firmware/src/protocole.md`: Toio制御プロトコル
- `toio_control/pc_tools/swarm_control/README.md`: Swarm Controlプロトコル

**確認ポイント**:
- ✅ メッセージ形式（アドレス、引数）
- ✅ 送受信の方向
- ✅ タイミング要件
- ✅ エラー処理

---

## 実装の流れ

### 音響制御の流れ

1. **Scheduler**: タイムラインJSONを読み込み
2. **SchedulerController**: OSCバンドルを生成
3. **OscSender**: 暗号化してUDP送信
4. **Firmware OscReceiver**: 受信・復号
5. **PlaybackQueue**: タイムラインに基づいて再生

### Toio制御の流れ

1. **M5**: `/announce`を送信 → PC側でデバイス登録
2. **M5**: `/toio/position`を送信 → PC側で座標受信
3. **Swarm Control**: 群れ行動シミュレーション
4. **OscSender**: `/toio/{robotIndex}/target`を送信
5. **M5**: 目標座標を受信 → Toioを制御

---

## トラブルシューティング

### よくある問題

1. **暗号化エラー**
   - 確認: `osc_config.json`のキー/IVが一致しているか
   - 確認: カウンタのリセット（送信ごとにインクリメント）

2. **メッセージが届かない**
   - 確認: ポート番号の設定
   - 確認: ブロードキャスト設定
   - 確認: ファイアウォール設定

3. **タイムライン同期の問題**
   - 確認: NTP同期の状態
   - 確認: Timetagの処理

**詳細**: `toio_control/docs/troubleshooting.md` を参照

---

## 関連ドキュメント

- **OSC契約仕様**: `toio_control/docs/osc_contract.md`
- **Toio制御プロトコル**: `m5-toio/firmware/src/protocole.md`
- **Swarm Control README**: `toio_control/pc_tools/swarm_control/README.md`
- **トラブルシューティング**: `toio_control/docs/troubleshooting.md`
- **デバイスセットアップ**: `toio_control/docs/device_setup.md`

---

## まとめ

OSC通信の実装は以下のように整理されています：

1. **基盤ライブラリ**: `toio_control/pc_tools/libs/osc/` - 送受信・暗号化の共通実装
2. **音響制御**: Scheduler → Firmware（`/acoustics/*`）
3. **Toio制御**: Swarm Control ↔ M5（`/toio/*`）
4. **統合**: Unified Applicationで統一OSC送受信器を使用

実装を確認する際は、上記の「実装確認のポイント」を参照してください。

