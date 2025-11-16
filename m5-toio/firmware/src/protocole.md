# PC - M5 間 OSC プロトコル叩き台

## 1. 目的と前提
- PC 側から M5（toio 中継機）へスキャン／接続／制御指令を送り、M5 から状態通知を OSC で返す。
- 接続対象は toio 名称のサフィックス（`toio-xxx` の `xxx`）で指定する。
- トランスポートは UDP/OSC（アドレス＋引数）。文字列は UTF-8、数値は 32bit 整数または浮動小数。

## 2. コマンド（PC → M5）
- `/scan`  
  - 引数なし。スキャン実行。結果は `/scan/result` で通知。
- `/connect` `<string suffix>`  
  - 指定サフィックスに接続。結果は `/connect/result` で通知。
- `/led` `<int r>` `<int g>` `<int b>`  
  - 0-255 の RGB で LED 設定。
- `/motor` `<int left>` `<int right>`  
  - -100..100 の左右速度指令。
- `/goal/set` `<float x>` `<float y>` `<float stop_distance>`  
  - ゴール追従開始。単位 mm。`stop_distance` 省略時は 20mm。
- `/goal/clear`  
  - ゴール追従を停止。
- `/status/request`  
  - 現在の姿勢/バッテリー/LED/モータ状態を単発で要求。応答は `/status`。

## 3. 通知（M5 → PC）
- `/scan/result` `<int count>` `[<string suffix> ...]`  
  - スキャン完了。`count` は件数、続けて suffix を列挙。
- `/connect/result` `<string suffix>` `<string status>` `<string message>`  
  - `status`: `"connected"` / `"not_found"` / `"failed"` など。
- `/status`  
  - 引数: `<int has_pose>` `<int x>` `<int y>` `<int angle>` `<int on_mat>` `<int has_batt>` `<int batt>` `<int led_r>` `<int led_g>` `<int led_b>` `<int motor_l>` `<int motor_r>`  
  - `has_pose`/`has_batt` は 0/1。座標 mm、角度度、バッテリー %。
- `/goal/reached` `<float x>` `<float y>`  
  - ゴール到達時に通知（任意）。
- `/error` `<string message>`  
  - プロトコル解釈エラーなど。

## 4. 状態遷移の目安
1. PC `/scan` → M5 スキャン中 → `/scan/result`
2. PC `/connect suffix` → 接続試行 → `/connect/result`
3. 接続後、必要に応じ `/status/request`、`/led`、`/motor`、`/goal/set`

## 5. 未決定/要調整
- ポート番号、送受信先 IP
- 連続ステータス通知の周期（必要なら `/status/subscribe` を追加）
- エラー時の詳細コード種別
