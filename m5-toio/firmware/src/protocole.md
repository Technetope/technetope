# PC - M5 WebSocket プロトコル案（シンプルJSON版）

OSC叩き台をWebSocket/JSONに置き換えた、フラットなメッセージ案です。`type` を直接示し、`id` で要求と応答を対応付けます。

## 1. 接続とメッセージ形式
- プロトコル: WebSocket
- エンドポイント例: `ws://<M5_IP>:9000/ws`
- 文字コード/形式: UTF-8 JSON 1メッセージ/行
- 共通フィールド（推奨）:
  - `type`: メッセージ種別（下記）
  - `id`: リクエストID（応答/通知でも可能な限り返すと対応付けが楽）
  - 必要に応じて `ts`（送信時刻）など任意フィールド追加可能
- サーバは接続直後に `{ "type": "hello", "id": "hello" }` を送るだけでも良い。

## 2. コマンド（PC → M5）
- `scan`  
  - 例: `{ "type": "scan", "id": "req1" }`  
  - スキャン実行。結果は `scan-result` 通知。
- `connect`  
  - 例: `{ "type": "connect", "id": "req2", "suffix": "A3Q" }`
- `led`  
  - 例: `{ "type": "led", "id": "req3", "r":0, "g":255, "b":128 }`
- `motor`  
  - 例: `{ "type": "motor", "id": "req4", "left":20, "right":20 }` （-100..100 想定）
- `goal-set`  
  - 例: `{ "type": "goal-set", "id": "req5", "x":100.0, "y":50.0, "stop_distance":20.0 }`
- `goal-clear`  
  - 例: `{ "type": "goal-clear", "id": "req6" }`
- `status-request`  
  - 例: `{ "type": "status-request", "id": "req7" }` 単発で状態取得
- `status-subscribe`  
  - 例: `{ "type": "status-subscribe", "id": "req8", "enable": true }` 購読ON/OFF

## 3. 応答・通知（M5 → PC）
- `scan-result`  
  - 例: `{ "type": "scan-result", "id": "req1", "count":2, "suffixes":["A3Q","B7Z"] }`
- `connect-result`  
  - 例: `{ "type": "connect-result", "id": "req2", "suffix":"A3Q", "status":"connected", "message":"" }`  
    - `status`: `"connected" | "not_found" | "failed" | "timeout"`
- `status`  
  - 例: `{ "type": "status", "id": "req7", "x":100, "y":50, "angle":90, "on_mat":1, "batt":85, "led":[0,255,128], "motor":[20,20] }`  
  - 購読時は `id` 省略可。`status-subscribe` ONなら姿勢/バッテリー更新でプッシュ。
- `goal-reached`  
  - 例: `{ "type": "goal-reached", "x":100.0, "y":50.0 }`
- `error`  
  - 例: `{ "type": "error", "id": "reqX", "message": "detail" }` （パースエラー・不正パラメータなど）

## 4. 状態遷移の目安
1. 接続: サーバ → `{ "type": "hello" }`
2. PC → `scan` → サーバ → `scan-result`
3. PC → `connect` → サーバ → `connect-result`
4. 接続後、必要に応じ `status-request` / `status-subscribe` / `led` / `motor` / `goal-set`
5. `status-subscribe` ON中は `status` をプッシュ。`goal-set` 実行中に到達したら `goal-reached`

## 5. 実装メモ
- 未知フィールドは無視可能にする。
- `id` は必須ではないが、リクエスト/応答対応付けに推奨。
- クライアント切断時に購読解除。複数クライアントがある場合は接続ごとに購読を持つ実装とする。
- 帯域削減が必要なら、必要最低限のフィールドに絞るかCBOR化を検討。
