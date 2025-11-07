# 実装分担 (最新版)

## Codex-A: 制御サーバー担当
1. プロジェクト骨組み (`CMakeLists.txt`, `include/`, `src/`, `third_party/`) を整備
2. 主要クラス実装: `relay_connection`, `relay_manager`, `cube_registry`, `fleet_orchestrator`, `ws_server`, `command_gateway`, `util/*`, `app/main`
3. `config_loader` で `field.top_left/bottom_right` を読み込み、デフォルト `(45,45)`→`(455,455)` を適用
4. CubeRegistry に LED 状態 `{r,g,b}` を保持し、`set_led` 実行時や Relay 応答に合わせて更新。`cube_update` / `snapshot` に `led` フィールドを含める
5. WebSocket (`command_gateway` + `ws_server`) を拡張し、`field_info` / `snapshot.field` / `cube_update.led` を配信
6. `docs/relay_json_spec.md`, `docs/control_webui_ws_spec.md` と整合をとりつつ `cmake -B build && cmake --build build` でビルド確認し、README に手順を追記
7. ゴール追従ロジックの段階的実装
   - (a) `CubeRegistry` から位置・LED・状態を抽出するユーティリティを用意し、`FleetOrchestrator` から周期的に参照できるようにする。必要に応じて `tick()` などの更新フックを追加。
   - (b) 単純なモーション生成器（仮称 `motion_controller`）を作成し、「回頭 → 直進 → 停止」など基本的なコマンド列を導出できるようにする。出力形式は `manual_drive` コマンド群で扱える速度と持続時間に分割。
   - (c) `FleetOrchestrator` に実行キュー管理を追加し、`set_goal` 受信時にモーションを生成してキューへ登録。進行中のゴールがある場合の優先度や差し替えルールを整理する。
   - (d) 実行ループ（タイマー）を導入し、所定周期でキュー先頭のコマンドを `RelayManager::send_manual_drive` へ送信。実時間・経過距離・ACK 等をトリガーに次のステップへ進め、必要に応じて停止コマンドを送る。
   - (e) ゴール到達判定を実装する。閾値（位置誤差・向き誤差）を定め、到達時にはキューを完了扱いし、`cube_update.goal_id` をクリア、`fleet_state` とログへ反映する。
   - (f) フェイルセーフと割り込み処理を追加する。リレー断・手動操作・新しいゴール投入時には現在のコマンドを中断し、キューブを停止させる。状態遷移とログ出力を忘れずに。
   - (g) 将来的な経路計画や衝突回避に備えて、モーション生成器と `FleetOrchestrator` の責務分離、設定値（速度・閾値等）の外部化を検討する。

## Codex-B: WebUI担当
1. `webui/index.html`, `styles.css`, `main.js` でビルドレス SPA を実装（Design.md の要件に準拠）
2. WebSocket で `/ws/ui` に接続し、`field_info` / `snapshot.field` を受けて Canvas のスケール・原点を動的に設定
3. `cube_update.led` / `snapshot.cubes[].led` を用いて Cube グリッドやマップの色を更新し、LED フォーム（RGB のみ）と双方向に連動させる
4. 必要に応じて `scripts/mock_control_server.js` などを用意し、`field_info` や LED 更新を含むテストデータを配信できるようにする
5. WebUI の開発・起動手順を README に記載し、追加ライブラリなしの Vanilla JS/CSS を維持。UI 仕様に追加があれば `DESIGN.md` / `docs` を更新

共通: `DESIGN.md` と各仕様 (`docs/*`) を常に参照し、変更があれば同時に更新する。
