# OSC Contract

PC 側（scheduler/monitor）と M5StickC Plus2 ファームウェアが共有する OSC メッセージ仕様。送信は原則 PC → デバイス、応答と心拍はデバイス → PC。

## 共通事項
- トランスポート: UDP。既定ポートは受信 9000 (`OSC_LISTEN_PORT`)、心拍送信 9100 (`HEARTBEAT_REMOTE_PORT`)。  
- 暗号化: PC → M5 は AES-CTR 256bit。平文先頭に 8 バイトのカウンタ（BE）を付け、`osc_config.json` の Key/IV を元に `IV + counter` で復号する。カウンタ 0 や鍵/IV 不整合は破棄して `[OSC]` ログ。M5 → PC は平文。  
- Timetag: `/acoustics/play` `/acoustics/stop` はバンドル timetag（NTP秒/分解能）を最優先。なければ第2引数で上書き可。どれも無ければ「受信時刻+0.5s」。過去時刻は現在に切り上げ。  
  - 第2引数の解釈優先度: `time` (OSC Timetag) → `int64` (Unix epoch マイクロ秒) → `int32` (受信時刻からの相対 ms)。  
- プリセット検証: ID が `manifest.json` に無い場合は無視し `[OSC] Unknown preset requested` を出力。

## PC → M5StickC (制御コマンド)

### `/acoustics/play`
| 引数 | 型 | 必須 | 意味 |
| --- | --- | --- | --- |
| 0 | string | Yes | プリセット ID (`manifest.json` の `id`) |
| 1 | time/int64/int32 | No | 実行時刻（上記優先度）。 |
| 2 | float | No | ゲイン 0.0–1.0。未指定 1.0。 |
| 3 | int32 | No | ループ指定。0 以外でループ。未指定 0。 |

処理: 指定時刻で再生キューに追加し、`StickCP2.Speaker` で再生。

### `/acoustics/stop`
| 引数 | 型 | 必須 | 意味 |
| --- | --- | --- | --- |
| 0 | — | — | （なし） |
| 1 | time/int64/int32 | No | 停止発動時刻。無指定は受信+0.5s。 |

処理: 再生キューに停止イベントを挿入し、指定時刻で再生とキューを停止。

## M5StickC → PC

### `/announce`
| 引数 | 型 | 意味 |
| --- | --- | --- |
| 0 | string | device_id |
| 1 | string | MAC アドレス |
| 2 | string | firmware バージョン |

用途: 初回心拍前に1度送信し、PC側レジストリへ登録。

### `/heartbeat`
NTP 同期後に 1s 間隔で送信。

| 引数 | 型 | 意味 |
| --- | --- | --- |
| 0 | string | device_id |
| 1 | int32 | シーケンス番号（0始まり） |
| 2 | int32 | 現在時刻（秒, Unix epoch） |
| 3 | int32 | 現在時刻（マイクロ秒部, 0–999,999） |
| 4 | int32 | 再生キューサイズ |
| 5 | int32 | 再生中フラグ (1=playing / 0=idle) |

## 運用メモ
- 復号失敗、カウンタ異常、バンドルパース失敗、未知プリセットは捨ててシリアルログに記録。ACK は無いので PC 側は送信結果と心拍有無を監視する。  
- 仕様変更時は `acoustics/tests/osc_sync_results.md` とツール実装（scheduler/monitor）を同時に更新すること。
