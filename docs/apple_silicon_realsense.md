# Apple Silicon macOS で RealSense D415 を扱うためのメモ

Apple Silicon Mac 上で Intel RealSense D415 を動かす際に発生した事象と、その回避策をまとめました。Viewer が落ちる／CLI がデバイスを掴めない、といった状況に遭遇したらここを参照してください。

## 動作確認済み環境

- **ハード**: Apple Silicon Mac + Intel RealSense D415  
- **Homebrew**: `/opt/homebrew`（arm64 bottle）で `librealsense` / `libusb` を導入  
- **FW バージョン**: 5.16.0.1（推奨は 5.17.0.10）  
- **主なツール**: `/opt/homebrew/bin/rs-enumerate-devices`, `rs-capture`, `rs-record`, `realsense-viewer`

> **注意**: SDK を arm64 / Rosetta でビルドし直しても、最終的に macOS の USB 制限がボトルネックになります。

## 目的

1. カメラを安定して列挙できる状態にする  
2. Viewer が不安定でも CLI でストリーミング／録画ができるようにする  
3. macOS で必要になるセキュリティ設定（DriverKit / 権限付与）を把握する

## まず実行する「儀式」

```zsh
# 0) 可能なら Mac 本体の USB-C に直挿し（ハブを外す）

# 1) macOS のカメラ常駐プロセスを止める
sudo killall VDCAssistant AppleCameraAssistant 2>/dev/null || true

# 2) デバイスが再認識されたら即 sudo でツールを起動
sudo /opt/homebrew/bin/rs-enumerate-devices
sudo /opt/homebrew/bin/rs-capture
```

- **sudo は必須**: 非 sudo だと `RS2_USB_STATUS_ACCESS` や `libusb_init failed` が出やすい  
- **USB2/USB3 の判別**: すべてのストリームが 15 FPS 前後で頭打ちなら USB2（480 Mb/s）接続の可能性が高い。ケーブルやポートを変えて `system_profiler SPUSBDataType` で “Up to 5 Gb/s” を確認する。

## DriverKit を使うかどうか

| アプローチ | 長所 | 短所 | 使うタイミング |
| ---------- | ---- | ---- | -------------- |
| **Intel DriverKit を導入**（公式 DMG の `IntelRealSenseSDK.pkg` → プライバシー&セキュリティで許可 → 再起動） | AppleUSBHost 経由で Viewer 等も安定 | Intel のシステム拡張を許可する必要がある | GUI ベースで使いたい / Apple 謹製ドライバと共存したい |
| **DriverKit 無しで libusb のみ** | 追加インストール不要 | Apple Silicon では `AppleUSBHostUserClient::openGated … provider is already opened` が頻発 | 他に手段が無い場合のみ（sudo + 儀式で CLI 運用） |
| **Rosetta ビルド** | Intel Mac に近い挙動で viewer を動かせる | USB 権限問題は依然として残る | GLSL まわりで arm64 が不安定な場合の保険 |

## よくあるエラーと意味

| エラー / 症状 | 意味 | 対処 |
| ------------- | ---- | ---- |
| `libusb_init failed: LIBUSB_ERROR_OTHER` | OS が libusb にホストコントローラを開かせていない | sudo 実行、Apple カメラ常駐プロセスを停止、DriverKit を許可 |
| `failed to set power state` / `failed to claim usb interface` | macOS が UVC をすでに掴んでいる | `sudo killall VDCAssistant AppleCameraAssistant` → 抜き差し → 即起動 |
| `AppleUSBHostUserClient::openGated … provider is already opened` | DriverKit クライアントしかアクセスできない | 公式 DriverKit をインストールして許可する |
| GLSL `#version 150 is not supported` | macOS が GLSL 1.20 の互換プロファイルしか返していない | `imgui_impl_opengl3.cpp` を修正し、ドライバの GLSL バージョンにフォールバック（本リポでは対処済み） |
| `Frame didn't arrive within 15000 ms` の後に 15 FPS で安定 | 初回タイムアウト。USB2 で速度制限されている | USB3 接続を確認（system_profiler） |

## ファームウェアと依存ライブラリ

```zsh
# FW 更新（sudo 必須）
sudo rs-fw-update -l
sudo rs-fw-update -f /path/to/D415_Firmware_5.17.0.10.bin

# 依存の更新
brew update
brew upgrade libusb librealsense
```

FW 更新後は必ずケーブルを抜き差ししてからテストする。

## 最小テストセット

```zsh
/opt/homebrew/bin/rs-enumerate-devices      # 非 sudo: 失敗しても想定内
sudo /opt/homebrew/bin/rs-enumerate-devices # 詳細が出れば OK
sudo /opt/homebrew/bin/rs-hello-realsense   # 生存確認
sudo /opt/homebrew/bin/rs-capture           # プレビュー（USB2 なら ≈15 FPS）
sudo /opt/homebrew/bin/rs-record -a out.bag # 録画
```

Viewer が安定しない場合でも、CLI で録画／ストリーム取得は可能。

## 毎回のおすすめ手順

1. `sudo killall VDCAssistant AppleCameraAssistant 2>/dev/null || true`  
2. カメラを抜き差しして **Mac 本体の USB-C ポートに直挿し**  
3. すぐに sudo で `rs-capture` や `rs-record` を実行

これで「claim できない」「segfault する」といった症状はほぼ再現しなくなった。

### sudo ラッパースクリプト例

CLI を頻繁に実行する場合、以下のようなスクリプトで「sudo の確保 → macOS カメラプロセス停止 → USB3 チェック → `rs-enumerate-devices` → `rs-capture`/`rs-record`」をまとめておくと便利です。

```zsh
mkdir -p ~/bin
cat > ~/bin/realsense-run.sh <<'SH'
#!/bin/zsh
set -euo pipefail
PATH=/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin

sudo -v || exit 1

sudo /usr/bin/killall VDCAssistant 2>/dev/null || true
sudo /usr/bin/killall AppleCameraAssistant 2>/dev/null || true

if /usr/sbin/system_profiler SPUSBDataType | awk '/Intel RealSense/{f=1} f && /Speed:/{print; exit}' | grep -vq '5 Gb/s'; then
  echo "[WARN] USB2っぽい接続（480Mb/s）。USB3ケーブル/直挿し推奨。"
fi

if ! sudo /opt/homebrew/bin/rs-enumerate-devices >/dev/null 2>&1; then
  echo "[ERROR] デバイス列挙に失敗。配線/権限を確認してね。"
  exit 2
fi

if [[ "${1:-}" == "--record" ]]; then
  out="${2:-$HOME/RealSense_$(date +%Y%m%d-%H%M%S).bag}"
  echo "[INFO] 録画開始 -> $out"
  exec sudo /usr/bin/caffeinate -dimsu /opt/homebrew/bin/rs-record -a "$out"
else
  echo "[INFO] プレビュー起動（終了は Ctrl+C）"
  exec sudo /usr/bin/caffeinate -dimsu /opt/homebrew/bin/rs-capture
fi
SH
chmod +x ~/bin/realsense-run.sh
```

`~/bin/realsense-run.sh` を実行すると、引数なしで `rs-capture`、`--record <path>` で `rs-record` が起動します。自前のツール（`calibrate_intrinsics` など）を走らせる前にこのスクリプトで列挙が通る状態にしておくと、librealsense の Segmentation fault を避けられます。

## USB 診断コマンド

```zsh
# USB2/USB3 の速度を確認
system_profiler SPUSBDataType | rg -A3 -i realsense

# USB や RealSense 関連のエラーをリアルタイム表示
log stream --style syslog \
  --predicate 'process CONTAINS[c] "usb" || eventMessage CONTAINS[c] "realsense"' \
  --level error
```

`libusb_init` で失敗した直後にこれらを確認すると、誰がハブを専有しているかが分かります。

---

新しいノウハウ（特定 macOS バージョン固有の挙動やハブの相性など）が分かったら、このファイルに追記してください。Apple Silicon 環境で同じ問題にぶつかる人の助けになります。***
