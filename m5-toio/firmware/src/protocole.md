# PC - M5 間 OSC プロトコル叩き台

## 1. 目的と前提
- PC 側から M5（toio 中継機）へスキャン／接続／制御指令を送り、M5 から状態通知を OSC で返す。
- 接続対象は toio 名称のサフィックス（`toio-xxx` の `xxx`）で指定する。
- トランスポートは UDP/OSC（アドレス＋引数）。文字列は UTF-8、数値は 32bit 整数または浮動小数。
- ネットワーク前提: PC 側のみ各 M5(toio 中継) の IP/ポートを把握し、指令を送る。M5 は送信元 IP/ポートへ返信するだけで PC の IP を保持しない（必要ならセッションID等で多重化）。

## 2. コマンド（PC → M5） ※すべて先頭に `/toio` プレフィックスを付与
- `/toio/scan`  
  - 引数なし。スキャン実行。結果は `/toio/scan-result` で通知。
- `/toio/connect` `<string suffix>`  
  - 指定サフィックスに接続。結果は `/toio/connect/result` で通知。
- `/toio/led` `<int r>` `<int g>` `<int b>`  
  - 0-255 の RGB で LED 設定。
- `/toio/motor` `<int left>` `<int right>`  
  - -100..100 の左右速度指令。
- `/toio/goal-set` `<float x>` `<float y>` `<float stop_distance>`  
  - ゴール追従開始。単位 toioマット。`stop_distance` 省略時は 20。
- `/toio/goal-clear`  
  - ゴール追従を停止。
- `/toio/status-request`  
  - 現在の姿勢/バッテリー/LED/モータ状態を単発で要求。応答は `/toio/status`。
- `/toio/status-subscribe` `<int enable>`  
  - `enable=1` で購読開始、`0` で購読停止。購読中は姿勢更新（`pose_dirty`）とバッテリー更新（`battery_dirty`）で `/toio/status` を送信する。

## 3. 通知（M5 → PC） ※先頭 `/toio` プレフィックス
- `/toio/scan-result` `<int count>` `[<string suffix> ...]`  
  - スキャン完了。`count` は件数、続けて suffix を列挙。
- `/toio/connect-result` `<string suffix>` `<string status>` `<string message>`  
  - `status`: `"connected"` / `"not_found"` / `"failed"` など。
- `/toio/status`  
  - 引数: `<int x>` `<int y>` `<int angle>` `<int on_mat>` `<int batt>` `<int led_r>` `<int led_g>` `<int led_b>` `<int motor_l>` `<int motor_r>`  
  - 座標: toio 座標系（マット基準）、角度: 度、バッテリー: %。
- `/toio/goal-reached` `<float x>` `<float y>`  
  - ゴール到達時に通知（任意）。
- `/toio/error` `<string message>`  
  - プロトコル解釈エラーなど。

## 4. 状態遷移の目安
1. PC `/toio/scan` → M5 スキャン中 → `/toio/scan-result`
2. PC `/toio/connect suffix` → 接続試行 → `/toio/connect-result`
3. 接続後、必要に応じ `/toio/status-request`、`/toio/led`、`/toio/motor`、`/toio/goal-set`

## 5. 未決定/要調整
- ポート番号、送受信先 IP（PC/M5 いずれがサーバになるか）
- エラー時の詳細コード種別（`status`/`message` の候補を固定するかどうか）
- `/toio/status` の送信タイミング：購読中は姿勢更新（`pose_dirty`）またはバッテリー更新（`battery_dirty`）で送る。
- ゴール到達や接続失敗時のリトライ方針・通知の有無
