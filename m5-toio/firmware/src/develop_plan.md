## M5 WebSocketサーバー移行プラン（main.cpp改修方針）

1) ネットワーク初期化を追加  
- Wi-Fi接続処理を `setup()` 冒頭に追加（SSID/PASSは定数/シークレット）。  
- 接続成功後にIPをシリアル/画面へ表示。

2) WebSocketサーバーの起動  
- ライブラリ選定: ArduinoWebsockets（同期poll型、単一クライアント前提でシンプル）。  
- `setup()` で `server.listen(9000)`、接続時に `hello` を送るハンドラ登録。`loop()` で `server.poll()` を短周期で呼ぶ。

3) JSONパース/生成の準備  
- ArduinoJsonを使い、共通バッファ（StaticJsonDocument）を用意。  
- 受信→`type`で分岐→応答を生成、という流れを関数化。

4) コマンドハンドラ実装（WebSocket/JSON）  
- `scan`: `g_toio.scan()` 実行 → `scan-result`返却。  
- `connect`: `connectBySuffix(suffix)` → `connect-result`。  
- `led` / `motor`: `setLedColor` / `driveMotor` → 成否を即応答。  
- `goal-set` / `goal-clear`: 既存の目標設定/停止関数呼び出し。  
- `status-request`: 現状態を単発送信。  
- `status-subscribe`: 購読フラグON/OFF。`poseDirty`/`batteryDirty`検知で`status`通知。

5) ループ再構成  
- `g_toio.loop()` とUI更新は維持しつつ、WS受信処理を追加（同期なら `server.poll()`、非同期ならコールバック）。  
- 購読ON時、dirtyフラグで `status` をプッシュ。  
- 初期の自動スキャン/接続/テスト送信は不要なら削除し、WSコマンド待ちにする。

6) エラーハンドリング  
- JSONパース失敗・未知 `type` は `error` を返す。  
- Wi-Fi未接続時は早期リターンか再接続を試行。

7) ディレクトリ構成と疎結合設計  
- `src/net/`: Wi-Fi接続とWebSocketサーバ起動/pollのみ。プロトコル知識を持たず、受信文字列と送信コールバックでやり取り。  
- `src/protocol/`: JSONパース/生成と`type`分岐。具体的なtoio制御は`commands`に委譲。  
- `src/commands/`: toio制御ラッパ（scan/connect/led/motor/goal-set/status購読）。購読状態フラグを保持してレスポンスデータを返す。  
- `src/ui/`: 表示/ログのみ。状態更新用のメソッドを提供し、ネット/プロトコルには依存しない。  
- `main.cpp`: 初期化と各モジュールの接着だけに絞る。依存注入（関数ポインタ/インタフェース）でモジュール間の結合を最小化。

8) 実装の具体ステップ  
- `platformio.ini` に ArduinoWebsockets と ArduinoJson を追加。  
- `src/net/`, `src/protocol/`, `src/commands/` にヘッダ/ソースひな型を置き、インタフェースを定義する。  
- `main.cpp`: Wi-Fi接続→WSリッスン→`poll()`呼び出しを追加。IP表示をUIに反映。自動スキャン/接続は外し、WSコマンド待ちへ。  
- `protocol`: 受信文字列をJSONパースし、`type`で分岐して`commands`を呼ぶ。応答JSONをシリアライズして送信コールバックへ渡す。  
- `commands`: toio API呼び出し＋購読フラグ管理。`status-request`は単発返却、`status-subscribe` ONでフラグを立てる。  
- ループ: `g_toio.loop()`＋UI更新＋`server.poll()`＋dirty検知で`status`プッシュ。購読OFFで停止。  
- 動作確認: `scan`/`connect`/`status-request` から試し、順次 `led`/`motor`/`goal-set` を確認。
