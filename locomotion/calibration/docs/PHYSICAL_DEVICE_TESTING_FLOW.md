# 実機テスト詳細フロー

**対象**: locomotion/calibration モジュール  
**対象カメラ**: Intel RealSense D415  
**対象プラットフォーム**: Apple Silicon macOS (M1/M2/M3), Linux  
**最終更新**: 2025-11-09

---

## 目次

1. [概要](#1-概要)
2. [テストフロー全体像](#2-テストフロー全体像)
3. [フェーズ0: 環境セットアップ](#3-フェーズ0-環境セットアップ)
4. [フェーズ1: 初回セットアップテスト](#4-フェーズ1-初回セットアップテスト)
5. [フェーズ2: 基本機能テスト](#5-フェーズ2-基本機能テスト)
6. [フェーズ3: 詳細機能テスト](#6-フェーズ3-詳細機能テスト)
7. [フェーズ4: 精度検証テスト](#7-フェーズ4-精度検証テスト)
8. [フェーズ5: 統合テスト](#8-フェーズ5-統合テスト)
9. [フェーズ6: 回帰テスト](#9-フェーズ6-回帰テスト)
10. [テスト結果記録](#10-テスト結果記録)
11. [トラブルシューティング](#11-トラブルシューティング)

---

## 1. 概要

このドキュメントは、RealSense D415カメラとtoioプレイマットを使用したキャリブレーションシステムの実機テストを、段階的かつ体系的に実施するための詳細なフローを提供します。

### 1.1 テストの目的

1. **機能検証**: 各機能が仕様通りに動作することを確認
2. **精度検証**: 座標変換の精度が要件を満たすことを確認
3. **安定性検証**: 長時間実行時の安定性を確認
4. **回帰検証**: 変更後の動作確認

### 1.2 テスト環境の前提条件

- **ハードウェア**: Intel RealSense D415、toioプレイマット、ChArUcoボード
- **ソフトウェア**: macOS 13.0+ または Linux (Ubuntu 20.04+)
- **カメラ設置**: 高さ2400-2800mm（推奨: 2500-2700mm）、床面に対して垂直

### 1.3 テスト実行のタイミング

- **初回セットアップ時**: 環境構築後の動作確認
- **機能追加時**: 新機能の動作確認
- **バグ修正時**: 修正内容の動作確認
- **リリース前**: 全機能の動作確認

---

## 2. テストフロー全体像

```
┌─────────────────────────────────────────────────────────────┐
│                    フェーズ0: 環境セットアップ                │
│  - ハードウェア接続確認                                       │
│  - ソフトウェア環境構築                                       │
│  - 設定ファイル準備                                           │
└─────────────────────────┬───────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────────┐
│              フェーズ1: 初回セットアップテスト                │
│  - カメラ接続確認                                             │
│  - ビルド確認                                                 │
│  - 基本動作確認                                               │
└─────────────────────────┬───────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────────┐
│                  フェーズ2: 基本機能テスト                    │
│  - ChArUco検出テスト                                          │
│  - キャリブレーション実行テスト                                │
│  - 結果出力テスト                                             │
└─────────────────────────┬───────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────────┐
│                  フェーズ3: 詳細機能テスト                    │
│  - インタラクティブツールテスト                                │
│  - QCツールテスト                                             │
│  - 座標変換テスト                                             │
└─────────────────────────┬───────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────────┐
│                  フェーズ4: 精度検証テスト                    │
│  - 再投影誤差検証                                             │
│  - 座標変換精度検証                                           │
│  - 再現性検証                                                 │
└─────────────────────────┬───────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────────┐
│                    フェーズ5: 統合テスト                      │
│  - 人間検出システム統合テスト                                  │
│  - MotionPlanner統合テスト                                    │
│  - エンドツーエンドテスト                                     │
└─────────────────────────┬───────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────────┐
│                    フェーズ6: 回帰テスト                      │
│  - 変更前後の比較テスト                                        │
│  - パフォーマンステスト                                       │
│  - 長時間実行テスト                                           │
└─────────────────────────────────────────────────────────────┘
```

---

## 3. フェーズ0: 環境セットアップ

### 3.1 ハードウェアセットアップ

#### ステップ1: カメラ接続確認

**実行コマンド:**
```bash
# macOS: カメラプロセスの停止
sudo killall VDCAssistant AppleCameraAssistant 2>/dev/null || true

# RealSenseデバイスの列挙（sudo必須、macOS）
# 注意: Apple Silicon環境では、Rosetta2経由での実行が必要な場合があります
# エラーが発生する場合は、以下のいずれかを試してください：
#   1. ネイティブARM64版: sudo /opt/homebrew/bin/rs-enumerate-devices
#   2. Rosetta2経由(x86_64): arch -x86_64 sudo /opt/homebrew/bin/rs-enumerate-devices
sudo /opt/homebrew/bin/rs-enumerate-devices

# または、Rosetta2経由で実行（エラーが発生する場合）
# arch -x86_64 sudo /opt/homebrew/bin/rs-enumerate-devices

# USB接続の確認（macOS）
# 注意: system_profilerで表示される場合と表示されない場合がある
# rs-enumerate-devicesで検出されれば問題なし
system_profiler SPUSBDataType | grep -i -A 5 "realsense\|intel.*0AD3\|USB.*Camera"
# または、より詳細な情報を取得
system_profiler SPUSBDataType | grep -i -B 2 -A 10 "0AD3\|realsense"
```

**確認項目:**
- [ ] RealSense D415が検出される
- [ ] USB接続が確認される（USB 3.0推奨、USB 2.1でも動作可能）
- [ ] カメラのシリアル番号が記録されている
- [ ] ファームウェアバージョンが5.16.0.1以上である
- [ ] ストリームプロファイルが表示される（Depth 640x480 @ 30/15/6 Hz、Color 640x480 @ 30/15/6 Hzなど）

**実際の出力例:**

**rs-enumerate-devices の出力:**
```
Device info: 
    Name                          :     Intel RealSense D415
    Serial Number                 :     038522062737
    Firmware Version              :     5.16.0.1
    Recommended Firmware Version  :     5.17.0.10
    Physical Port                 :     2-1.1.3-7
    Debug Op Code                 :     15
    Advanced Mode                 :     YES
    Product Id                    :     0AD3
    Camera Locked                 :     YES
    Usb Type Descriptor           :     2.1
    Product Line                  :     D400
    Asic Serial Number            :     043423020284
    Firmware Update Id            :     043423020284
    Connection Type               :     USB

Stream Profiles supported by Stereo Module
 Supported modes:
    STREAM      RESOLUTION     FORMAT      FPS
    Depth       640x480       Z16         @ 30/15/6 Hz
    ...

Stream Profiles supported by RGB Camera
 Supported modes:
    STREAM      RESOLUTION     FORMAT      FPS
    Color       640x480       RGB8        @ 30/15/6 Hz
    ...
```

**system_profiler の出力例（検出される場合）:**
```
Intel(R) RealSense(TM) Depth Camera 415 :

  Product ID: 0x0ad3
  Vendor ID: 0x8086  (Intel Corporation)
  Version: 51.00
  Speed: Up to 480 Mb/s
  Manufacturer: Intel(R) RealSense(TM) Depth Camera 415 
  Location ID: 0x02113000 / 3
  Current Available (mA): 500
  Current Required (mA): 496
  Extra Operating Current (mA): 0
```

**重要なポイント:**
- `Speed: Up to 480 Mb/s` はUSB 2.0の速度（USB 3.0は "Up to 5 Gb/s" と表示）
- これは `rs-enumerate-devices` の `Usb Type Descriptor: 2.1` と一致
- **USB 2.0接続の場合**: フレームレートが制限されるため、専用の設定ファイル（`calibration_config_usb2.json`）を使用する必要があります
- **USB 3.0接続の場合**: より高いフレームレート（15fps以上）が可能で、標準の設定ファイル（`calibration_config_low_res.json`）を使用できます

**重要な確認ポイント:**
1. **USB Type Descriptor / Speed**: 
   - `rs-enumerate-devices`で `Usb Type Descriptor: 2.1` と表示される場合: **USB 2.0/2.1接続**
     - **対処法**: `calibration_config_usb2.json`（6fps）を使用してください
     - USB 2.0の帯域幅（480 Mb/s）では、640x480@15fpsのカラー+深度ストリームを同時に送信するのは帯域幅不足です
     - 6fpsに下げることで、USB 2.0でも安定して動作します
   - `system_profiler`で `Speed: Up to 480 Mb/s` と表示される場合: **USB 2.0接続**（480 Mb/s = USB 2.0の最大速度）
     - **対処法**: `calibration_config_usb2.json`（6fps）を使用してください
   - `system_profiler`で `Speed: Up to 5 Gb/s` と表示される場合: **USB 3.0接続**（推奨）
     - **対処法**: `calibration_config_low_res.json`（15fps）または`calibration_config.json`（15fps）を使用できます
     - USB 3.0では、より高いフレームレート（15fps、30fps）が可能です
   - **注意**: `rs-enumerate-devices`でカメラが検出されれば、`system_profiler`で表示されなくても問題なし
   - `system_profiler`はシステムレベルのUSB認識を表示するが、RealSense SDKは直接USBインターフェースにアクセスするため、認識方法が異なる

2. **ファームウェアバージョン**:
   - `Firmware Version`: 現在のファームウェアバージョン（5.16.0.1以上推奨）
   - `Recommended Firmware Version`: 推奨ファームウェアバージョン（異なる場合、更新を検討）

3. **エラーメッセージ**:
   - `ERROR [0x...] (dispatcher.cpp:34) Dispatcher [...] exception caught: mutex lock failed: Invalid argument` というエラーが表示される場合:
     - **これは正常な動作です**: カメラ情報の列挙には成功している場合、このエラーは無視しても問題ありません
     - このエラーは、RealSense SDKの内部的なスレッド同期処理で発生するもので、カメラの動作には影響しません
     - カメラ情報（Device info、Stream Profiles）が正常に表示されていれば、カメラは正常に検出されています
     - カメラが正常に動作するか確認（実際のキャリブレーション実行で確認）
     - 問題が続く場合は、カメラの抜き差しやシステム再起動を試す

4. **ストリームプロファイル**:
   - `Depth 640x480 Z16 @ 30/15/6 Hz` が表示されることを確認
   - `Color 640x480 RGB8 @ 30/15/6 Hz` が表示されることを確認
   - これらのプロファイルが利用可能であることを確認

**トラブルシューティング:**
- カメラが検出されない場合:
  - USBケーブルを抜き差し
  - Mac本体のUSB-Cポートに直挿し（ハブ経由でない）
  - 他のUSBデバイスを外して再試行
  - システムを再起動
- **Apple Silicon環境でのUSBアクセスエラー**（以下のエラーが表示される場合）:
  ```
  ERROR [0x...] (handle-libusb.h:127) failed to claim usb interface: 0, error: RS2_USB_STATUS_ACCESS
  ERROR [0x...] (uvc-sensor.cpp:428) acquire_power failed: failed to set power state
  Could not create device - failed to set power state
  segmentation fault
  ```
  - **原因**: RealSense SDKがx86_64版しかなく、Apple Siliconで直接実行できない場合
  - **対処法**: Rosetta2経由で実行する
    ```bash
    # Rosetta2経由で実行（x86_64バイナリの場合）
    arch -x86_64 sudo /opt/homebrew/bin/rs-enumerate-devices
    
    # または、実行アーキテクチャを確認
    file /opt/homebrew/bin/rs-enumerate-devices
    # x86_64と表示される場合、Rosetta2経由での実行が必要
    ```
  - **実行アーキテクチャの確認方法**:
    ```bash
    # RealSense SDKのアーキテクチャを確認
    file /opt/homebrew/bin/rs-enumerate-devices
    
    # システムのアーキテクチャを確認
    uname -m
    # arm64 と表示される場合、Apple Silicon環境
    
    # Rosetta2が有効か確認
    arch
    # arm64 と表示される場合、ネイティブARM64モード
    ```
  - **推奨**: キャリブレーションツールも同じアーキテクチャで実行する
    ```bash
    # Rosetta2経由でキャリブレーション実行（x86_64バイナリの場合）
    arch -x86_64 sudo ./build/capture_calibration calibration_config.json calib_result.json
    ```
- USB 2.0/2.1で接続されている場合:
  - `rs-enumerate-devices`で `Usb Type Descriptor: 2.1` と表示される
  - `system_profiler`で `Speed: Up to 480 Mb/s` と表示される（USB 2.0の最大速度）
  - **対処法**:
    - USB 3.0ポートに接続し直す（可能な場合）
    - USB 3.0ケーブルを使用（可能な場合）
    - USB 2.0/2.1でも動作するが、フレームレートが制限される可能性がある（15 FPS以下）
    - 実際のキャリブレーション実行で動作を確認
    - USB 3.0接続に変更すると、より高いフレームレート（30 FPS）が可能になる
- エラーメッセージが表示される場合（カメラ情報は表示される）:
  - カメラ情報が表示されていれば、カメラは検出されている
  - `ERROR [0x...] (dispatcher.cpp:34) Dispatcher [...] exception caught: mutex lock failed: Invalid argument` というエラーは**正常な動作**です
  - このエラーは、RealSense SDKの内部的なスレッド同期処理で発生するもので、カメラの動作には影響しません
  - カメラ情報とストリームプロファイルが正常に表示されていれば、問題ありません
  - 実際のキャリブレーション実行で動作を確認
  - 問題が続く場合は、カメラの抜き差しやシステム再起動を試す
- **system_profilerでRealSenseが表示される場合**:
  - 正常に検出されている場合、以下のような情報が表示されます:
    ```
    Intel(R) RealSense(TM) Depth Camera 415 :
      Product ID: 0x0ad3
      Vendor ID: 0x8086  (Intel Corporation)
      Speed: Up to 480 Mb/s  (USB 2.0) または Up to 5 Gb/s (USB 3.0)
    ```
  - `Speed: Up to 480 Mb/s` はUSB 2.0接続を示します
  - `Speed: Up to 5 Gb/s` はUSB 3.0接続を示します（推奨）
- **system_profilerでRealSenseが表示されない場合**:
  - `system_profiler SPUSBDataType | grep -A 5 "Intel RealSense"` が何も出力しない場合でも、`rs-enumerate-devices`でカメラが検出されれば問題ありません
  - `system_profiler`はシステムレベルのUSB認識を表示しますが、RealSense SDKは直接USBインターフェースにアクセスするため、認識方法が異なります
  - より詳細な情報を取得する場合:
    ```bash
    # Product ID (0AD3) で検索
    system_profiler SPUSBDataType | grep -i -B 2 -A 10 "0AD3"
    
    # または、すべてのUSBデバイスを表示
    system_profiler SPUSBDataType
    
    # または、ioregを使用
    ioreg -p IOUSB -l -w 0 | grep -i "realsense\|0AD3"
    ```
  - **重要な確認**: `rs-enumerate-devices`でカメラが検出されれば、`system_profiler`で表示されなくても問題ありません
- ファームウェアバージョンが推奨バージョンと異なる場合:
  - 現在のファームウェアバージョンが5.16.0.1以上であれば動作可能
  - 推奨ファームウェアバージョンへの更新を検討（必須ではない）
  - ファームウェア更新手順: Intel RealSense SDKのドキュメントを参照

#### ステップ2: カメラ設置確認

**確認項目:**
- [ ] カメラがtoioプレイマットの真上に設置されている（高さ: 2400-2800mm、推奨: 2500-2700mm）
- [ ] カメラが床面に対して垂直（傾き ≤ 2°）
- [ ] カメラの視野がプレイマット全体をカバーしている
- [ ] 照明条件が適切（均一な明るさ、グレア・影を避ける）

**測定方法:**
```bash
# カメラ高さの測定（メジャーまたはレーザー距離計を使用）
# 動作範囲: 2400-2800mm（設定ファイルのfloor_z_min_mm ~ floor_z_max_mm）
# 推奨範囲: 2500-2700mm（警告なし）
# 実際の測定例: 2400mm（240cm）でも動作可能
# 注意: 2400-2500mmの範囲では警告が表示される可能性があるが、動作には問題なし

# カメラの傾き確認（レベル計を使用）
# 目標: 床面に対して垂直（傾き ≤ 2°）
```

**カメラ高さの詳細:**
- **動作範囲**: 2400-2800mm（設定ファイルの`floor_z_min_mm` ~ `floor_z_max_mm`に基づく）
- **推奨範囲**: 2500-2700mm（警告なし、`camera_height_warn_min_mm` ~ `camera_height_warn_max_mm`）
- **実際の測定例**: 2400mm（240cm）でも動作可能
- **注意事項**:
  - 2400-2500mmの範囲では警告が表示される可能性があるが、動作には問題ありません
  - 高さが低いほど視野が狭くなり、プレイマット全体をカバーできない可能性があります
  - 高さが高いほど深度データの精度が低下する可能性があります
  - カメラ高さは、プレイマット全体を視野に収められる範囲で設定してください

#### ステップ2-5: カメラ画像取得テスト（簡易確認）

**目的**: カメラから画像が正常に取得できることを確認

**実行コマンド:**

**重要: USB接続速度に応じて適切な設定ファイルを選択してください**

- **USB 2.0接続の場合**: `calibration_config_usb2.json`（6fps）を使用
- **USB 3.0接続の場合**: `calibration_config_low_res.json`（15fps）を使用

```bash
# カメラプロセスの停止
sudo killall VDCAssistant AppleCameraAssistant 2>/dev/null || true

# 方法1: RealSense SDKの標準ツールを使用（最も簡単）
# rs-capture: カラー画像と深度画像を表示
sudo /opt/homebrew/bin/rs-capture

# または、Rosetta2経由で実行（エラーが発生する場合）
# arch -x86_64 sudo /opt/homebrew/bin/rs-capture

# 方法2: インタラクティブツールを使用（GUI表示）
# 重要: 設定ファイルのパス解決のため、calibrationディレクトリから実行する必要があります
cd /Users/ksk432/technetope/locomotion/calibration

# USB 2.0接続の場合（6fps）
arch -x86_64 sudo ./build/capture_calibration_interactive \
  calibration_config_usb2.json \
  /tmp/test_calib_result.json

# USB 3.0接続の場合（15fps）
# arch -x86_64 sudo ./build/capture_calibration_interactive \
#   calibration_config_low_res.json \
#   /tmp/test_calib_result.json
```

**USB 2.0とUSB 3.0の違い:**
- **USB 2.0（480 Mb/s）の場合**:
  - 帯域幅が限られているため、フレームレートを6fpsに制限する必要があります
  - `calibration_config_usb2.json`を使用してください
  - 表示が少し遅く感じる可能性がありますが、キャリブレーションの精度には影響しません（解像度は同じ640x480）
- **USB 3.0（5 Gb/s）の場合**:
  - より高い帯域幅があるため、15fpsや30fpsが可能です
  - `calibration_config_low_res.json`（15fps）または`calibration_config.json`（15fps）を使用できます
  - より滑らかな表示が可能です

**期待される結果:**
- カメラから画像が取得できる
- カラー画像と深度画像が表示される（`rs-capture`の場合）
- リアルタイムプレビューが表示される（インタラクティブツールの場合）
  - **USB 2.0の場合**: 約6 FPS（`calibration_config_usb2.json`使用時）
  - **USB 3.0の場合**: 約10-15 FPS（`calibration_config_low_res.json`使用時）

**確認項目:**
- [ ] カメラから画像が取得できる
- [ ] カラー画像が表示される
- [ ] 深度画像が表示される（`rs-capture`の場合）
- [ ] リアルタイムプレビューが表示される（インタラクティブツールの場合）
  - [ ] USB 2.0の場合: 約6 FPSで表示される（`calibration_config_usb2.json`使用時）
  - [ ] USB 3.0の場合: 約10-15 FPSで表示される（`calibration_config_low_res.json`使用時）
- [ ] エラーメッセージが表示されない（特に「Frame didn't arrive within 15000」エラーが発生しない）

**操作方法（`rs-capture`の場合）:**
- ウィンドウが開き、カラー画像と深度画像が表示される
- `Ctrl+C`で終了

**操作方法（インタラクティブツールの場合）:**
- ウィンドウが開き、リアルタイムプレビューが表示される
- `Q`または`ESC`キーで終了
- `D`キーで検出オーバーレイのトグル
- `S`キーでデバッグフレーム保存
- **重要**: ウィンドウを全画面にしない（フレーム取得がタイムアウトする可能性があります）

**トラブルシューティング:**
- **設定ファイルが見つからないエラー**:
  ```
  [WARN] Config file "calibration_config_low_res.json" not found. Using defaults.
  [warning] Failed to load playmat layout 'config/toio_playmat.json': Failed to open playmat layout file
  ```
  - **原因**: 実行ディレクトリが間違っている（設定ファイルのパス解決が失敗）
  - **対処法**: `calibration`ディレクトリから実行する
    ```bash
    # 正しいディレクトリに移動
    cd /Users/ksk432/technetope/locomotion/calibration
    
    # その後、ツールを実行
    sudo ./build/capture_calibration_interactive \
      calibration_config_low_res.json \
      /tmp/test_calib_result.json
    ```
- **「Couldn't resolve requests」エラー**:
  ```
  [error] Failed to start RealSense pipeline: Couldn't resolve requests
  ```
  - **原因**: 設定ファイルが読み込まれていない、またはカメラの設定が不正
  - **対処法**:
    - 正しいディレクトリから実行する（上記参照）
    - 設定ファイルが存在するか確認: `ls -la calibration_config_low_res.json`
    - カメラプロセスを停止: `sudo killall VDCAssistant AppleCameraAssistant 2>/dev/null || true`
    - カメラを抜き差しして再接続
- 画像が表示されない場合:
  - カメラプロセスを停止: `sudo killall VDCAssistant AppleCameraAssistant 2>/dev/null || true`
  - USB接続を確認
  - カメラの電源を確認
  - システムを再起動
- **「Frame didn't arrive within 15000」エラーが発生する場合**:
  - **原因**: フレーム取得がタイムアウト（15秒以内にフレームが取得できない）
  - **対処法**:
    - **USB 2.0接続の場合**: `calibration_config_usb2.json`（6fps）を使用してください
      - USB 2.0の帯域幅では、15fpsの設定では帯域幅不足でタイムアウトが発生します
      - 6fpsに下げることで、USB 2.0でも安定して動作します
    - ウィンドウを全画面にしない（ウィンドウサイズを適切なサイズに保つ）
    - カメラプロセスを停止: `sudo killall VDCAssistant AppleCameraAssistant 2>/dev/null || true`
    - カメラを抜き差しして再接続
    - USB接続を確認（USB 3.0推奨、USB 2.0の場合は上記の設定ファイルを使用）
    - システムを再起動
    - 他のアプリケーションがカメラを使用していないか確認
- **全画面表示時の問題**:
  - ウィンドウを全画面にすると、フレーム取得がタイムアウトする可能性があります
  - **推奨**: ウィンドウサイズを適切なサイズ（例: 1280×720、1920×1080）に保つ
  - 全画面表示が必要な場合は、ウィンドウをリサイズして使用してください
- `rs-capture`が存在しない場合:
  - RealSense SDKが正しくインストールされているか確認: `brew list librealsense`
  - または、インタラクティブツールを使用
- Apple Silicon環境でエラーが発生する場合:
  - Rosetta2経由で実行: `arch -x86_64 sudo /opt/homebrew/bin/rs-capture`

#### ステップ3: ChArUcoボード準備

**実行コマンド:**
```bash
# ChArUcoボード画像の生成（必要に応じて）
cd /Users/ksk432/technetope/locomotion/calibration
python3 tools/generate_charuco_board.py \
  --output charuco_board_5x7_45mm.png \
  --metadata charuco_board_5x7_45mm.json

# 印刷ガイドPDFの生成（推奨）
python3 tools/generate_printing_guide.py \
  --output charuco_board_printing_guide.pdf
```

**確認項目:**
- [ ] ChArUcoボードが印刷されている（A3用紙、100%スケール）
- [ ] 実寸確認: チェス盤マスが45mm × 45mm（目標値）
- [ ] 実寸確認: ArUcoマーカーが33mm × 33mm（目標値）
- [ ] ボードが平面で固定されている（歪み・折れがない）
- [ ] ボードがtoioプレイマット上に配置されている（中央推奨）
- [ ] ボードが床面に対して平行（傾き ≤ 5°）

**測定方法:**
```bash
# 実寸確認（定規を使用）
# チェス盤マス: 45mm × 45mm（目標値）
# ArUcoマーカー: 33mm × 33mm（目標値）
# ボード全体: 180mm × 270mm（余白除く、5x7グリッド、45mmマスの場合）

# 実際の測定値が異なる場合:
# 例: チェス盤マスが50mm × 50mmで印刷された場合
# → 設定ファイルのcharuco_square_length_mmを50.0に変更する必要があります
```

**トラブルシューティング:**
- **実寸が合わない場合（例: チェス盤マスが50mmで印刷された場合）**:
  - **重要**: 設定ファイルの値を実際の印刷サイズに合わせて変更する必要があります
  - 設定ファイルの変更方法:
    ```bash
    # 設定ファイルを編集
    # calibration_config.json または calibration_config_low_res.json
    {
      "charuco_square_length_mm": 50.0,  # 実際の測定値に変更（例: 50mm）
      "charuco_marker_length_mm": 37.0,  # 比例して変更（33mm × 50/45 ≈ 36.67mm、四捨五入して37mm）
      ...
    }
    ```
  - マーカーサイズの計算:
    - チェス盤マスが50mmの場合: マーカーサイズ = 33mm × (50/45) ≈ 36.67mm → 37mm（四捨五入）
    - または、実際のマーカーサイズを測定して設定
  - ボード全体サイズの確認:
    - 5x7グリッド、50mmマスの場合: 幅 = (5-1) × 50mm = 200mm、高さ = (7-1) × 50mm = 300mm
  - プリンタのスケール設定を確認（100%になっているか）
  - 用紙サイズの設定を確認
  - プリンタの「ページに合わせる」などの自動調整機能を無効化
  - スクリプトのパラメータを調整して再生成（推奨）:
    ```bash
    # 50mmマスで再生成
    python3 tools/generate_charuco_board.py \
      --square-length-mm 50.0 \
      --marker-length-mm 37.0 \
      --output charuco_board_5x7_50mm.png \
      --metadata charuco_board_5x7_50mm.json
    ```
  - **注意**: 設定ファイルの値を変更した後、キャリブレーションを再実行してください

#### ステップ4: toioプレイマット準備

**確認項目:**
- [ ] toioプレイマットが平らに配置されている
- [ ] プレイマットが固定されている（動かない）
- [ ] プレイマットのサイズが正しい（A3: 420mm × 297mm）
- [ ] プレイマットのID範囲が正しい（Start: (34, 35), End: (339, 250)）

**測定方法:**
```bash
# プレイマットの実寸確認（メジャーを使用）
# A3 Simple Playmat #01: 420mm × 297mm

# toio Position ID座標の確認（toioキューブを使用）
# Start: (34, 35)
# End: (339, 250)
```

### 3.2 ソフトウェアセットアップ

#### ステップ1: 依存ライブラリのインストール

**macOS (Apple Silicon):**
```bash
# 依存ライブラリのインストール
brew install librealsense opencv spdlog nlohmann-json

# インストール確認
brew list librealsense opencv spdlog nlohmann-json
```

**Linux (Ubuntu):**
```bash
# 依存ライブラリのインストール
sudo apt update
sudo apt install librealsense2-dev libopencv-dev libspdlog-dev nlohmann-json3-dev

# インストール確認
dpkg -l | grep -E "librealsense2|libopencv|libspdlog|nlohmann-json"
```

**確認項目:**
- [ ] librealsense2がインストールされている（バージョン2.54.0以上）
- [ ] OpenCVがインストールされている（バージョン4.5.0以上、arucoモジュール含む）
- [ ] spdlogがインストールされている
- [ ] nlohmann/jsonがインストールされている

**トラブルシューティング:**
- OpenCVのarucoモジュールが含まれていない場合:
  - `brew install opencv`（Homebrew版はcontribを同梱）
  - または、OpenCV contribを別途インストール
- librealsense2がインストールできない場合:
  - Apple Silicon環境では、Homebrewでインストール可能
  - または、ソースからビルド（詳細は[APPLE_SILICON_COMPATIBILITY.md](../APPLE_SILICON_COMPATIBILITY.md)を参照）
- **Apple Silicon環境でのRealSense SDKのアーキテクチャ確認**:
  - RealSense SDKがx86_64版しかない場合、Rosetta2経由での実行が必要
  - 確認方法:
    ```bash
    # RealSense SDKのアーキテクチャを確認
    file /opt/homebrew/bin/rs-enumerate-devices
    # x86_64 と表示される場合、Rosetta2経由での実行が必要
    # arm64 と表示される場合、ネイティブARM64版（Rosetta2不要）
    ```
  - Rosetta2経由で実行する場合:
    ```bash
    # Rosetta2経由で実行
    arch -x86_64 sudo /opt/homebrew/bin/rs-enumerate-devices
    # キャリブレーションツールも同じアーキテクチャで実行
    arch -x86_64 sudo ./build/capture_calibration ...
    ```
  - 詳細は、[APPLE_SILICON_COMPATIBILITY.md](../APPLE_SILICON_COMPATIBILITY.md)を参照

#### ステップ2: ビルド環境の準備

**実行コマンド:**
```bash
cd /Users/ksk432/technetope/locomotion/calibration

# ビルドディレクトリの作成
cmake -B build -DLOCOMOTION_BUILD_TESTS=ON

# ビルド実行
cmake --build build

# ビルド結果の確認
ls -la build/*calibration* build/*qc
```

**確認項目:**
- [ ] ビルドが成功する（エラーなし）
- [ ] 以下の実行ファイルが生成される:
  - [ ] `build/capture_calibration`（CLIツール）
  - [ ] `build/capture_calibration_interactive`（インタラクティブツール）
  - [ ] `build/run_calibration_qc`（QCツール）
  - [ ] `build/monitor_human_detection`（人間検出モニター）
- [ ] テスト実行ファイルが生成される（`LOCOMOTION_BUILD_TESTS=ON`の場合）:
  - [ ] `build/test_floor_plane_estimator`
  - [ ] `build/test_calibration_pipeline_integration`

**トラブルシューティング:**
- ビルドエラーが発生する場合:
  - 依存ライブラリが正しくインストールされているか確認
  - CMakeのバージョンを確認（3.20以上必要）
  - コンパイラのバージョンを確認（C++20対応必要）
  - ビルドログを確認してエラー内容を特定

#### ステップ3: 設定ファイルの準備

**実行コマンド:**
```bash
# 設定ファイルの確認
cat config/calibration_config.json

# 低解像度設定（テスト用）の確認
cat calibration_config_low_res.json

# toioプレイマットレイアウトの確認
cat config/toio_playmat.json
```

**確認項目:**
- [ ] `config/calibration_config.json`が存在する（USB 3.0用、15fps）
- [ ] `calibration_config_low_res.json`が存在する（USB 3.0用、15fps、テスト用）
- [ ] `calibration_config_usb2.json`が存在する（USB 2.0用、6fps）
- [ ] `config/toio_playmat.json`が存在する
- [ ] 設定ファイルの内容が正しい:
  - [ ] `playmat_layout_path`: "config/toio_playmat.json"
  - [ ] `board_mount_label`: "center_mount_nominal"
  - [ ] `min_charuco_corners`: 12
  - [ ] `max_reprojection_error_id`: 8.0

**設定ファイルの選択:**
- **USB 2.0接続の場合**: `calibration_config_usb2.json`（6fps）を使用
  - USB 2.0の帯域幅（480 Mb/s）では、15fpsの設定では帯域幅不足でタイムアウトが発生します
  - 6fpsに下げることで、USB 2.0でも安定して動作します
- **USB 3.0接続の場合**: `calibration_config_low_res.json`（15fps）または`calibration_config.json`（15fps）を使用
  - USB 3.0では、より高いフレームレート（15fps、30fps）が可能です

**設定ファイルの重要な項目:**
```json
{
  "color_width": 640,
  "color_height": 480,
  "depth_width": 640,
  "depth_height": 480,
  "fps": 15,  // USB 2.0の場合は6、USB 3.0の場合は15
  "min_charuco_corners": 12,
  "max_reprojection_error_id": 8.0,
  "playmat_layout_path": "config/toio_playmat.json",
  "board_mount_label": "center_mount_nominal"
}
```

**トラブルシューティング:**
- 設定ファイルが存在しない場合:
  - デフォルト値が使用されることを確認
  - または、設定ファイルをコピーして作成
- パス解決エラーが発生する場合:
  - 相対パスが設定ファイル基準で解決されることを確認
  - または、絶対パスに変更

### 3.3 環境セットアップチェックリスト

**ハードウェア:**
- [ ] RealSense D415が接続されている
- [ ] USB 3.0接続である
- [ ] カメラが2400-2800mmの高さに設置されている（推奨: 2500-2700mm）
- [ ] カメラが床面に対して垂直である
- [ ] ChArUcoボードが印刷されている（実寸確認済み）
- [ ] ChArUcoボードがプレイマット上に配置されている
- [ ] toioプレイマットが平らに配置されている

**ソフトウェア:**
- [ ] 依存ライブラリがインストールされている
- [ ] ビルドが成功している
- [ ] 実行ファイルが生成されている
- [ ] 設定ファイルが準備されている

**環境:**
- [ ] 照明条件が適切である
- [ ] カメラの視野がプレイマット全体をカバーしている
- [ ] テスト環境が整備されている

---

## 4. フェーズ1: 初回セットアップテスト

### 4.1 カメラ接続テスト

#### テスト1-1: カメラ初期化テスト

**実行コマンド:**
```bash
cd /Users/ksk432/technetope/locomotion/calibration

# カメラプロセスの停止
sudo killall VDCAssistant AppleCameraAssistant 2>/dev/null || true

# カメラ初期化テスト（簡易版）
sudo /opt/homebrew/bin/rs-enumerate-devices
```

**期待される結果:**
- RealSense D415が検出される
- カメラの情報が表示される（シリアル番号、ファームウェアバージョンなど）

**確認項目:**
- [ ] カメラが検出される
- [ ] カメラの情報が正しく表示される
- [ ] エラーメッセージが表示されない

**トラブルシューティング:**
- カメラが検出されない場合:
  - USBケーブルを抜き差し
  - Mac本体のUSB-Cポートに直挿し
  - 他のUSBデバイスを外して再試行
  - システムを再起動

#### テスト1-2: フレーム取得テスト（カメラ画像確認）

**実行コマンド:**

**重要: USB接続速度に応じて適切な設定ファイルを選択してください**

- **USB 2.0接続の場合**: `calibration_config_usb2.json`（6fps）を使用
- **USB 3.0接続の場合**: `calibration_config_low_res.json`（15fps）を使用

```bash
# カメラプロセスの停止
sudo killall VDCAssistant AppleCameraAssistant 2>/dev/null || true

# 方法1: RealSense SDKの標準ツールを使用（最も簡単）
sudo /opt/homebrew/bin/rs-capture

# または、Rosetta2経由で実行（エラーが発生する場合）
# arch -x86_64 sudo /opt/homebrew/bin/rs-capture

# 方法2: インタラクティブツールを使用（GUI表示）
cd /Users/ksk432/technetope/locomotion/calibration

# USB 2.0接続の場合（6fps）
arch -x86_64 sudo ./build/capture_calibration_interactive \
  calibration_config_usb2.json \
  /tmp/test_calib_result.json

# USB 3.0接続の場合（15fps）
# arch -x86_64 sudo ./build/capture_calibration_interactive \
#   calibration_config_low_res.json \
#   /tmp/test_calib_result.json
```

**期待される結果:**
- カメラからフレームが取得できる
- カラー画像と深度画像が表示される（`rs-capture`の場合）
- リアルタイムプレビューが表示される（インタラクティブツールの場合）
  - **USB 2.0の場合**: 約6 FPS（`calibration_config_usb2.json`使用時）
  - **USB 3.0の場合**: 約10-15 FPS（`calibration_config_low_res.json`使用時）

**確認項目:**
- [ ] カラー画像が取得できる（表示される）
- [ ] 深度画像が取得できる（`rs-capture`の場合、表示される）
- [ ] リアルタイムプレビューが表示される（インタラクティブツールの場合）
  - [ ] USB 2.0の場合: 約6 FPSで表示される（`calibration_config_usb2.json`使用時）
  - [ ] USB 3.0の場合: 約10-15 FPSで表示される（`calibration_config_low_res.json`使用時）
- [ ] エラーメッセージが表示されない（特に「Frame didn't arrive within 15000」エラーが発生しない）

**操作方法:**
- `rs-capture`: ウィンドウが開き、カラー画像と深度画像が表示される。`Ctrl+C`で終了
- インタラクティブツール: ウィンドウが開き、リアルタイムプレビューが表示される。`Q`または`ESC`で終了

**トラブルシューティング:**
- 画像が表示されない場合:
  - カメラプロセスを停止: `sudo killall VDCAssistant AppleCameraAssistant 2>/dev/null || true`
  - USB接続を確認
  - カメラの電源を確認
  - システムを再起動
- `rs-capture`が存在しない場合:
  - RealSense SDKが正しくインストールされているか確認: `brew list librealsense`
  - または、インタラクティブツールを使用
- Apple Silicon環境でエラーが発生する場合:
  - Rosetta2経由で実行: `arch -x86_64 sudo /opt/homebrew/bin/rs-capture`

### 4.2 ビルド確認テスト

#### テスト1-3: ビルド結果確認

**実行コマンド:**
```bash
# ビルド結果の確認
ls -la build/capture_calibration
ls -la build/capture_calibration_interactive
ls -la build/run_calibration_qc
```

**確認項目:**
- [ ] 実行ファイルが存在する
- [ ] 実行ファイルに実行権限がある
- [ ] 実行ファイルのサイズが0でない

#### テスト1-4: ヘルプ表示テスト

**実行コマンド:**
```bash
# CLIツールのヘルプ表示
./build/capture_calibration --help

# インタラクティブツールのヘルプ表示
./build/capture_calibration_interactive --help

# QCツールのヘルプ表示
./build/run_calibration_qc --help
```

**期待される結果:**
- ヘルプメッセージが表示される
- 使用方法が説明される

**確認項目:**
- [ ] ヘルプメッセージが表示される
- [ ] 使用方法が説明される
- [ ] エラーメッセージが表示されない

### 4.3 基本動作確認テスト

#### テスト1-5: 設定ファイル読み込みテスト

**実行コマンド:**
```bash
# 設定ファイルの読み込みテスト（エラーチェック）
./build/capture_calibration \
  calibration_config_low_res.json \
  /tmp/test_calib_result.json 2>&1 | head -20
```

**期待される結果:**
- 設定ファイルが正しく読み込まれる
- エラーメッセージが表示されない（カメラ接続エラーは除く）

**確認項目:**
- [ ] 設定ファイルが正しく読み込まれる
- [ ] パス解決が正しく動作する
- [ ] エラーメッセージが表示されない（カメラ接続エラーは除く）

**トラブルシューティング:**
- パス解決エラーが発生する場合:
  - 設定ファイルのパスを確認
  - 相対パスが設定ファイル基準で解決されることを確認
  - または、絶対パスに変更

#### テスト1-6: カメラ接続テスト（実際のカメラを使用）

**実行コマンド:**
```bash
# カメラプロセスの停止
sudo killall VDCAssistant AppleCameraAssistant 2>/dev/null || true

# カメラ接続テスト（実際のカメラを使用）
sudo ./build/capture_calibration \
  calibration_config_low_res.json \
  /tmp/test_calib_result.json 2>&1 | head -50
```

**期待される結果:**
- カメラが初期化される
- フレームが取得できる
- ChArUcoボードの検出が試みられる

**確認項目:**
- [ ] カメラが初期化される
- [ ] フレームが取得できる
- [ ] ChArUcoボードの検出が試みられる
- [ ] エラーメッセージが表示されない（検出失敗は除く）

**トラブルシューティング:**
- カメラが初期化されない場合:
  - USB接続を確認
  - カメラプロセスを停止
  - システムを再起動
- フレームが取得できない場合:
  - カメラの電源を確認
  - USBケーブルを確認
  - カメラのファームウェアを更新

### 4.4 初回セットアップテストチェックリスト

**カメラ接続:**
- [ ] カメラが検出される
- [ ] カメラの情報が正しく表示される
- [ ] フレームが取得できる

**ビルド確認:**
- [ ] 実行ファイルが存在する
- [ ] ヘルプメッセージが表示される
- [ ] エラーメッセージが表示されない

**基本動作:**
- [ ] 設定ファイルが正しく読み込まれる
- [ ] パス解決が正しく動作する
- [ ] カメラが初期化される

---

## 5. フェーズ2: 基本機能テスト

### 5.1 ChArUco検出テスト

#### テスト2-1: ChArUcoボード検出テスト

**実行コマンド:**
```bash
cd /Users/ksk432/technetope/locomotion/calibration

# カメラプロセスの停止
sudo killall VDCAssistant AppleCameraAssistant 2>/dev/null || true

# ChArUcoボード検出テスト
sudo ./build/capture_calibration \
  calibration_config_low_res.json \
  /tmp/test_calib_result.json 2>&1 | tee /tmp/test_output.log
```

**期待される結果:**
- ChArUcoボードが検出される（12個以上のコーナー）
- 検出ログが表示される
- キャリブレーション結果が生成される

**確認項目:**
- [ ] ChArUcoボードが検出される
- [ ] 検出コーナー数が12個以上である
- [ ] 検出ログが表示される
- [ ] エラーメッセージが表示されない

**検証方法:**
```bash
# ログの確認
grep "ChArUco" /tmp/test_output.log
grep "corners detected" /tmp/test_output.log
grep "error" /tmp/test_output.log -i
```

**トラブルシューティング:**
- ChArUcoボードが検出されない場合:
  - ボードの位置を調整（カメラから40-80cmの距離）
  - 照明条件を改善（均一な明るさ、グレアを避ける）
  - ボードの傾きを確認（床面に対して平行）
  - ボードのサイズを確認（目標: 45mm正方形、33mmマーカー、実際の測定値に応じて設定ファイルを変更）
  - カメラの焦点距離を確認

#### テスト2-2: 検出コーナー数テスト

**実行コマンド:**
```bash
# 検出コーナー数の確認
sudo ./build/capture_calibration \
  calibration_config_low_res.json \
  /tmp/test_calib_result.json 2>&1 | grep "corners detected"
```

**期待される結果:**
- 検出コーナー数が12個以上である
- 検出コーナー数が安定している（複数回実行で変動が少ない）

**確認項目:**
- [ ] 検出コーナー数が12個以上である
- [ ] 検出コーナー数が安定している
- [ ] 検出コーナー数が最大値に近い（理想的には20個以上）

**検証方法:**
```bash
# 複数回実行して検出コーナー数を記録
for i in {1..5}; do
  echo "Run $i:"
  sudo ./build/capture_calibration \
    calibration_config_low_res.json \
    /tmp/test_calib_result_$i.json 2>&1 | grep "corners detected"
done
```

### 5.2 キャリブレーション実行テスト

#### テスト2-3: 基本キャリブレーション実行

**実行コマンド:**
```bash
# カメラプロセスの停止
sudo killall VDCAssistant AppleCameraAssistant 2>/dev/null || true

# キャリブレーション実行
# 注意: Apple Silicon環境でRealSense SDKがx86_64版の場合、Rosetta2経由で実行が必要
# エラーが発生する場合は: arch -x86_64 sudo ./build/capture_calibration ...
sudo ./build/capture_calibration \
  calibration_config_low_res.json \
  calib_result.json

# または、Rosetta2経由で実行（エラーが発生する場合）
# arch -x86_64 sudo ./build/capture_calibration \
#   calibration_config_low_res.json \
#   calib_result.json
```

**期待される結果:**
- キャリブレーションが成功する（exit code 0）
- キャリブレーション結果JSONが生成される
- 再投影誤差が許容範囲内である

**確認項目:**
- [ ] キャリブレーションが成功する（exit code 0）
- [ ] 出力JSONファイルが生成される
- [ ] JSONスキーマ v2.0 に準拠している
- [ ] `reprojection_error_px` が含まれている
- [ ] `toio_coordinate_transform` セクションが含まれている
- [ ] `validation` セクションが含まれている

**検証方法:**
```bash
# キャリブレーション結果の確認
cat calib_result.json | jq '.'

# 再投影誤差の確認
cat calib_result.json | jq '.quality_metrics.reprojection_error_px'
cat calib_result.json | jq '.quality_metrics.reprojection_error_id'

# 検証結果の確認
cat calib_result.json | jq '.validation'
```

**トラブルシューティング:**
- キャリブレーションが失敗する場合:
  - ChArUcoボードの検出を確認
  - 検出コーナー数が12個以上であることを確認
  - ボードの位置を調整
  - 照明条件を改善
- 再投影誤差が大きい場合:
  - ChArUcoボードの検出品質を改善
  - ボードの配置を調整（カメラに対して平行）
  - サブピクセル補正が有効であることを確認

#### テスト2-4: キャリブレーション結果検証

**実行コマンド:**
```bash
# キャリブレーション結果の検証
cat calib_result.json | jq '.validation'

# 品質メトリクスの確認
cat calib_result.json | jq '.quality_metrics'

# toio座標変換の確認
cat calib_result.json | jq '.toio_coordinate_transform'
```

**期待される結果:**
- 検証結果が `passed: true` である
- 品質メトリクスが許容範囲内である
- toio座標変換が正しく設定されている

**確認項目:**
- [ ] 検証結果が `passed: true` である
- [ ] 再投影誤差が ≤ 8.0 pixels である
- [ ] 再投影誤差が ≤ 8.0 ID units である
- [ ] 床面推定が成功している（標準偏差 ≤ 8.0mm）
- [ ] toio座標変換が正しく設定されている

**検証方法:**
```bash
# 検証結果の詳細確認
cat calib_result.json | jq '.validation.checks'
cat calib_result.json | jq '.validation.warnings'

# 品質メトリクスの詳細確認
cat calib_result.json | jq '.quality_metrics.reprojection_error_px'
cat calib_result.json | jq '.quality_metrics.reprojection_error_id'
cat calib_result.json | jq '.quality_metrics.reprojection_error_floor_mm'

# 床面推定の確認
cat calib_result.json | jq '.floor_plane.std_mm'
cat calib_result.json | jq '.floor_plane.inlier_ratio'

# toio座標変換の確認
cat calib_result.json | jq '.toio_coordinate_transform.transform_error_id'
cat calib_result.json | jq '.toio_coordinate_transform.coverage_area_toio_id'
```

### 5.3 結果出力テスト

#### テスト2-5: JSON出力テスト

**実行コマンド:**
```bash
# キャリブレーション結果JSONの確認
cat calib_result.json | jq '.schema_version'
cat calib_result.json | jq '.timestamp'
cat calib_result.json | jq '.camera'
cat calib_result.json | jq '.homography'
cat calib_result.json | jq '.floor_plane'
cat calib_result.json | jq '.quality_metrics'
cat calib_result.json | jq '.toio_coordinate_transform'
cat calib_result.json | jq '.validation'
```

**期待される結果:**
- JSONスキーマ v2.0 に準拠している
- すべての必須フィールドが含まれている
- データ型が正しい

**確認項目:**
- [ ] JSONスキーマ v2.0 に準拠している
- [ ] すべての必須フィールドが含まれている
- [ ] データ型が正しい
- [ ] JSONが正しくフォーマットされている

**検証方法:**
```bash
# JSONスキーマの確認
cat calib_result.json | jq '.schema_version'  # 期待値: "2.0"

# 必須フィールドの確認
cat calib_result.json | jq 'has("camera")'
cat calib_result.json | jq 'has("homography")'
cat calib_result.json | jq 'has("floor_plane")'
cat calib_result.json | jq 'has("quality_metrics")'
cat calib_result.json | jq 'has("toio_coordinate_transform")'
cat calib_result.json | jq 'has("validation")'

# JSONの構文チェック
cat calib_result.json | jq '.' > /dev/null && echo "JSON is valid" || echo "JSON is invalid"
```

### 5.4 基本機能テストチェックリスト

**ChArUco検出:**
- [ ] ChArUcoボードが検出される
- [ ] 検出コーナー数が12個以上である
- [ ] 検出コーナー数が安定している

**キャリブレーション実行:**
- [ ] キャリブレーションが成功する
- [ ] キャリブレーション結果JSONが生成される
- [ ] 再投影誤差が許容範囲内である

**結果出力:**
- [ ] JSONスキーマ v2.0 に準拠している
- [ ] すべての必須フィールドが含まれている
- [ ] データ型が正しい

---

## 6. フェーズ3: 詳細機能テスト

### 6.1 インタラクティブツールテスト

#### テスト3-1: インタラクティブツール起動テスト

**実行コマンド:**
```bash
# カメラプロセスの停止
sudo killall VDCAssistant AppleCameraAssistant 2>/dev/null || true

# インタラクティブツールの起動
# 重要: 設定ファイルのパス解決のため、calibrationディレクトリから実行する必要があります
cd /Users/ksk432/technetope/locomotion/calibration
sudo ./build/capture_calibration_interactive \
  calibration_config_low_res.json \
  calib_result.json

# または、Rosetta2経由で実行（エラーが発生する場合）
# arch -x86_64 sudo ./build/capture_calibration_interactive \
#   calibration_config_low_res.json \
#   calib_result.json
```

**期待される結果:**
- ウィンドウが開く
- カラー/深度プレビューが表示される
- ステータスパネルが表示される
- インストラクションパネルが表示される

**確認項目:**
- [ ] ウィンドウが正常に開く
- [ ] カラー/深度プレビューが表示される
- [ ] ステータスパネルが表示される
- [ ] インストラクションパネルが表示される
- [ ] リアルタイムプレビューが表示される（10-15 FPS）

**操作手順:**
1. ウィンドウが開くことを確認
2. カラー/深度プレビューが表示されることを確認
3. ステータスパネルが表示されることを確認
4. インストラクションパネルが表示されることを確認
5. **重要**: ウィンドウを全画面にしない（フレーム取得がタイムアウトする可能性があります）
6. `Q`または`ESC`キーで終了

**注意事項:**
- **全画面表示時の問題**: ウィンドウを全画面にすると、「Frame didn't arrive within 15000」エラーが発生する可能性があります
- **推奨**: ウィンドウサイズを適切なサイズ（例: 1280×720、1920×1080）に保つ
- 全画面表示が必要な場合は、ウィンドウをリサイズして使用してください

#### テスト3-2: ChArUco検出オーバーレイテスト

**実行コマンド:**
```bash
# インタラクティブツールの起動
# 重要: 設定ファイルのパス解決のため、calibrationディレクトリから実行する必要があります
cd /Users/ksk432/technetope/locomotion/calibration
sudo ./build/capture_calibration_interactive \
  calibration_config_low_res.json \
  calib_result.json

# または、Rosetta2経由で実行（エラーが発生する場合）
# arch -x86_64 sudo ./build/capture_calibration_interactive \
#   calibration_config_low_res.json \
#   calib_result.json
```

**操作手順:**
1. ウィンドウが開く
2. ChArUcoボードをカメラの視野内に配置
3. `D`キーを押して検出オーバーレイをトグル
4. ステータスインジケーターの色を確認

**期待される結果:**
- 検出オーバーレイが表示される
- コーナーが表示される
- ステータスインジケーターの色が変化する

**確認項目:**
- [ ] 検出オーバーレイが表示される（`D`キーでトグル）
- [ ] コーナーが表示される
- [ ] ステータスインジケーターの色が正しく変化する:
  - [ ] 🔴 Red: < 8コーナー検出
  - [ ] 🟡 Yellow: 8-11コーナー検出
  - [ ] 🟢 Green: ≥ 12コーナー検出（キャプチャ可能）

#### テスト3-3: キャプチャ実行テスト

**実行コマンド:**
```bash
# インタラクティブツールの起動
# 重要: 設定ファイルのパス解決のため、calibrationディレクトリから実行する必要があります
cd /Users/ksk432/technetope/locomotion/calibration
sudo ./build/capture_calibration_interactive \
  calibration_config_low_res.json \
  calib_result.json

# または、Rosetta2経由で実行（エラーが発生する場合）
# arch -x86_64 sudo ./build/capture_calibration_interactive \
#   calibration_config_low_res.json \
#   calib_result.json
```

**操作手順:**
1. ウィンドウが開く
2. ChArUcoボードをカメラの視野内に配置
3. ステータスが 🟢 Green になるまでボードを調整
4. `SPACE`キーでキャプチャ
5. "Capturing frame..." トーストが表示される
6. 処理完了後、結果が表示される

**期待される結果:**
- キャプチャが正常に実行される
- キャリブレーション結果JSONが生成される
- 結果が表示される

**確認項目:**
- [ ] キャプチャが正常に実行される（`SPACE`キー）
- [ ] "Capturing frame..." トーストが表示される
- [ ] 処理完了後、結果が表示される
- [ ] キャリブレーション結果JSONが生成される
- [ ] エラーメッセージが表示されない

#### テスト3-4: キーボードショートカットテスト

**実行コマンド:**
```bash
# インタラクティブツールの起動
# 重要: 設定ファイルのパス解決のため、calibrationディレクトリから実行する必要があります
cd /Users/ksk432/technetope/locomotion/calibration
sudo ./build/capture_calibration_interactive \
  calibration_config_low_res.json \
  calib_result.json

# または、Rosetta2経由で実行（エラーが発生する場合）
# arch -x86_64 sudo ./build/capture_calibration_interactive \
#   calibration_config_low_res.json \
#   calib_result.json
```

**操作手順:**
1. ウィンドウが開く
2. 各キーボードショートカットをテスト:
   - `SPACE`: キャプチャ実行
   - `S`: デバッグフレーム保存
   - `D`: 検出オーバーレイのトグル
   - `Q` / `ESC`: 終了

**期待される結果:**
- 各キーボードショートカットが正常に動作する

**確認項目:**
- [ ] `SPACE`キーでキャプチャが実行される
- [ ] `S`キーでデバッグフレームが保存される
- [ ] `D`キーで検出オーバーレイがトグルされる
- [ ] `Q` / `ESC`キーで終了する

### 6.2 QCツールテスト

#### テスト3-5: QCツール実行テスト

**実行コマンド:**
```bash
# カメラプロセスの停止
sudo killall VDCAssistant AppleCameraAssistant 2>/dev/null || true

# QCツールの実行
sudo ./build/run_calibration_qc \
  config/calibration_config.json \
  qc_report.md \
  qc_report.json
```

**期待される結果:**
- RealSense デバイス接続チェックが成功
- キャリブレーションが実行される
- 結果が検証される
- MarkdownレポートとJSONレポートが生成される

**確認項目:**
- [ ] デバイス接続チェックが成功
- [ ] キャリブレーションが実行される
- [ ] 検証結果が正しく判定される
- [ ] Markdownレポートが生成される
- [ ] JSONレポートが生成される
- [ ] レポートに推奨アクションが含まれている

**検証方法:**
```bash
# Markdownレポートの確認
cat qc_report.md

# JSONレポートの確認
cat qc_report.json | jq '.'

# 検証結果の確認
cat qc_report.json | jq '.validation'
cat qc_report.json | jq '.recommendations'
```

### 6.3 座標変換テスト

#### テスト3-6: 画像ピクセル → toio座標変換テスト

**実行コマンド:**
```bash
# キャリブレーション結果の確認
cat calib_result.json | jq '.toio_coordinate_transform'

# 座標変換のテスト（プログラムで実行）
# 注意: このテストは実装されている場合に実行
```

**期待される結果:**
- 画像ピクセル座標からtoio座標への変換が正常に動作する
- 変換誤差が許容範囲内である

**確認項目:**
- [ ] 画像ピクセル座標からtoio座標への変換が正常に動作する
- [ ] 変換誤差が ≤ 2.0 ID units である
- [ ] カバレッジエリアが正しく設定されている

#### テスト3-7: toio座標 → 画像ピクセル変換テスト

**実行コマンド:**
```bash
# キャリブレーション結果の確認
cat calib_result.json | jq '.toio_coordinate_transform'

# 座標変換のテスト（プログラムで実行）
# 注意: このテストは実装されている場合に実行
```

**期待される結果:**
- toio座標から画像ピクセル座標への変換が正常に動作する
- ラウンドトリップ誤差が許容範囲内である

**確認項目:**
- [ ] toio座標から画像ピクセル座標への変換が正常に動作する
- [ ] ラウンドトリップ誤差が ≤ 2.0 pixels である
- [ ] カバレッジエリアチェックが正常に動作する

### 6.4 詳細機能テストチェックリスト

**インタラクティブツール:**
- [ ] ウィンドウが正常に開く
- [ ] リアルタイムプレビューが表示される
- [ ] ステータスインジケーターが正しく色分けされる
- [ ] ChArUco検出オーバーレイが表示される
- [ ] キャプチャが正常に実行される
- [ ] キーボードショートカットが正常に動作する

**QCツール:**
- [ ] デバイス接続チェックが成功
- [ ] キャリブレーションが実行される
- [ ] 検証結果が正しく判定される
- [ ] レポートが生成される

**座標変換:**
- [ ] 画像ピクセル → toio座標変換が動作する
- [ ] toio座標 → 画像ピクセル変換が動作する
- [ ] カバレッジエリアチェックが動作する

---

## 7. フェーズ4: 精度検証テスト

### 7.1 再投影誤差検証

#### テスト4-1: 再投影誤差測定テスト

**実行コマンド:**
```bash
# キャリブレーション実行（複数回）
for i in {1..5}; do
  echo "Run $i:"
  sudo ./build/capture_calibration \
    calibration_config_low_res.json \
    calib_result_$i.json
  cat calib_result_$i.json | jq '.quality_metrics.reprojection_error_px'
  cat calib_result_$i.json | jq '.quality_metrics.reprojection_error_id'
done
```

**期待される結果:**
- 再投影誤差が ≤ 8.0 pixels である
- 再投影誤差が ≤ 8.0 ID units である
- 再投影誤差が安定している（複数回実行で変動が少ない）

**確認項目:**
- [ ] 再投影誤差（ピクセル）が ≤ 8.0 pixels である
- [ ] 再投影誤差（ID units）が ≤ 8.0 ID units である
- [ ] 再投影誤差が安定している
- [ ] 再投影誤差の標準偏差が ≤ 1.0 pixels である

**検証方法:**
```bash
# 再投影誤差の統計計算
echo "Reprojection Error Statistics:"
for i in {1..5}; do
  cat calib_result_$i.json | jq -r '.quality_metrics.reprojection_error_px'
done | awk '{sum+=$1; sumsq+=$1*$1; count++} END {print "Mean:", sum/count, "StdDev:", sqrt(sumsq/count - (sum/count)^2)}'
```

#### テスト4-2: 床面推定精度検証

**実行コマンド:**
```bash
# キャリブレーション結果の確認
cat calib_result.json | jq '.floor_plane.std_mm'
cat calib_result.json | jq '.floor_plane.inlier_ratio'
cat calib_result.json | jq '.camera.camera_height_mm'
```

**期待される結果:**
- 床面推定の標準偏差が ≤ 8.0mm である
- インライア比率が ≥ 0.7 である
- カメラ高さが 2400-2800mm の範囲内である（推奨: 2500-2700mm、警告なし）

**確認項目:**
- [ ] 床面推定の標準偏差が ≤ 8.0mm である
- [ ] インライア比率が ≥ 0.7 である
- [ ] カメラ高さが 2500-2700mm の範囲内である
- [ ] 床面推定が安定している

**検証方法:**
```bash
# 床面推定の統計計算
echo "Floor Plane Statistics:"
for i in {1..5}; do
  cat calib_result_$i.json | jq -r '.floor_plane.std_mm'
done | awk '{sum+=$1; sumsq+=$1*$1; count++} END {print "Mean:", sum/count, "StdDev:", sqrt(sumsq/count - (sum/count)^2)}'
```

### 7.2 座標変換精度検証

#### テスト4-3: toio座標変換精度テスト

**実行コマンド:**
```bash
# キャリブレーション結果の確認
cat calib_result.json | jq '.toio_coordinate_transform.transform_error_id'
cat calib_result.json | jq '.toio_coordinate_transform.coverage_area_toio_id'
```

**期待される結果:**
- 変換誤差が ≤ 2.0 ID units である
- カバレッジエリアが期待範囲内である

**確認項目:**
- [ ] 変換誤差が ≤ 2.0 ID units である
- [ ] カバレッジエリアが期待範囲内である
- [ ] ラウンドトリップ誤差が ≤ 2.0 pixels である（実装されている場合）

**検証方法:**
```bash
# 変換誤差の確認
cat calib_result.json | jq '.toio_coordinate_transform.transform_error_id'

# カバレッジエリアの確認
cat calib_result.json | jq '.toio_coordinate_transform.coverage_area_toio_id'

# 期待値との比較
# 期待値: {"min": {"x": 34.0, "y": 35.0}, "max": {"x": 339.0, "y": 250.0}}
# 許容誤差: ±1.0 ID units
```

#### テスト4-4: 既知点での精度検証

**実行コマンド:**
```bash
# 既知点での精度検証（手動テスト）
# 1. ChArUcoボードのコーナー位置を画像上で確認
# 2. 対応するtoio座標を計算
# 3. 期待値との誤差を確認
```

**期待される結果:**
- 既知点での誤差が ≤ 2.0 ID units である
- 複数点で一貫した精度が得られる

**確認項目:**
- [ ] 既知点での誤差が ≤ 2.0 ID units である
- [ ] 複数点で一貫した精度が得られる
- [ ] 座標変換が正しく動作する

**検証方法:**
1. ChArUcoボードの4つの角の位置を画像上で確認
2. 対応するtoio座標を計算（キャリブレーション結果を使用）
3. 期待値（`config/toio_playmat.json`の`correspondences`）と比較
4. 誤差を計算

### 7.3 再現性検証

#### テスト4-5: 再現性テスト

**実行コマンド:**
```bash
# キャリブレーション実行（複数回、同じ条件で）
for i in {1..5}; do
  echo "Run $i:"
  sudo ./build/capture_calibration \
    calibration_config_low_res.json \
    calib_result_$i.json
done

# 結果の比較
for i in {1..5}; do
  echo "Run $i:"
  cat calib_result_$i.json | jq '.quality_metrics.reprojection_error_px'
  cat calib_result_$i.json | jq '.quality_metrics.reprojection_error_id'
  cat calib_result_$i.json | jq '.floor_plane.std_mm'
done
```

**期待される結果:**
- 再投影誤差の標準偏差が ≤ 1.0 pixels である
- 床面推定の標準偏差が ≤ 2.0mm である
- 結果が一貫している

**確認項目:**
- [ ] 再投影誤差の標準偏差が ≤ 1.0 pixels である
- [ ] 床面推定の標準偏差が ≤ 2.0mm である
- [ ] 結果が一貫している
- [ ] ホモグラフィ行列が安定している

**検証方法:**
```bash
# 再投影誤差の統計計算
echo "Reprojection Error Statistics:"
for i in {1..5}; do
  cat calib_result_$i.json | jq -r '.quality_metrics.reprojection_error_px'
done | awk '{sum+=$1; sumsq+=$1*$1; count++} END {print "Mean:", sum/count, "StdDev:", sqrt(sumsq/count - (sum/count)^2)}'

# 床面推定の統計計算
echo "Floor Plane Statistics:"
for i in {1..5}; do
  cat calib_result_$i.json | jq -r '.floor_plane.std_mm'
done | awk '{sum+=$1; sumsq+=$1*$1; count++} END {print "Mean:", sum/count, "StdDev:", sqrt(sumsq/count - (sum/count)^2)}'
```

### 7.4 精度検証テストチェックリスト

**再投影誤差:**
- [ ] 再投影誤差が ≤ 8.0 pixels である
- [ ] 再投影誤差が ≤ 8.0 ID units である
- [ ] 再投影誤差が安定している

**床面推定:**
- [ ] 床面推定の標準偏差が ≤ 8.0mm である
- [ ] インライア比率が ≥ 0.7 である
- [ ] カメラ高さが 2500-2700mm の範囲内である

**座標変換:**
- [ ] 変換誤差が ≤ 2.0 ID units である
- [ ] カバレッジエリアが期待範囲内である
- [ ] 既知点での誤差が ≤ 2.0 ID units である

**再現性:**
- [ ] 再投影誤差の標準偏差が ≤ 1.0 pixels である
- [ ] 床面推定の標準偏差が ≤ 2.0mm である
- [ ] 結果が一貫している

---

## 8. フェーズ5: 統合テスト

### 8.1 人間検出システム統合テスト

#### テスト5-1: 人間検出モニター起動テスト

**実行コマンド:**
```bash
# カメラプロセスの停止
sudo killall VDCAssistant AppleCameraAssistant 2>/dev/null || true

# 人間検出モニターの起動
sudo ./build/monitor_human_detection \
  calib_result.json \
  config/human_detection_monitor.json
```

**期待される結果:**
- ウィンドウが3つのパネルで開く
- リアルタイムプレビューが表示される
- 人間検出が動作する

**確認項目:**
- [ ] ウィンドウが3つのパネルで開く
- [ ] パネル1（左）: 生のカラー画像
- [ ] パネル2（中央）: 人間検出結果のオーバーレイ
- [ ] パネル3（右）: toio座標上の可視化
- [ ] ステータス情報が表示される
- [ ] 操作説明が表示される

#### テスト5-2: 人間検出動作テスト

**実行コマンド:**
```bash
# 人間検出モニターの起動
sudo ./build/monitor_human_detection \
  calib_result.json \
  config/human_detection_monitor.json
```

**操作手順:**
1. 1人がtoioプレイマット上に立つ
2. 静止状態を維持（5-10秒）
3. モニターUIで以下を確認

**期待される結果:**
- 人間が検出される
- 頭位置と足位置が検出される
- トラッキングIDが割り当てられる
- toio座標が表示される

**確認項目:**
- [ ] 人間が検出される
- [ ] 頭位置（緑の円）が表示される
- [ ] 足位置（青の円）が表示される
- [ ] 楕円領域（黄色の楕円）が表示される
- [ ] トラッキングIDが表示される
- [ ] 動き状態が表示される（STANDING/MOVING）
- [ ] toio座標が表示される

### 8.2 MotionPlanner統合テスト

#### テスト5-3: DynamicObstacle変換テスト

**実行コマンド:**
```bash
# 人間検出モニターの起動
sudo ./build/monitor_human_detection \
  calib_result.json \
  config/human_detection_monitor.json
```

**期待される結果:**
- 人間検出結果がMotionPlannerのDynamicObstacleに変換される
- 未来位置の予測が含まれている
- 確信度が適切に設定されている

**確認項目:**
- [ ] `ConvertToDynamicObstacles`が正常に動作する
- [ ] 変換された`DynamicObstacle`が正しい形式である
- [ ] 未来位置の予測が含まれている
- [ ] 確信度が適切に設定されている

### 8.3 エンドツーエンドテスト

#### テスト5-4: エンドツーエンドテスト

**実行コマンド:**
```bash
# 1. キャリブレーション実行
sudo ./build/capture_calibration \
  calibration_config_low_res.json \
  calib_result.json

# 2. 人間検出モニターの起動
sudo ./build/monitor_human_detection \
  calib_result.json \
  config/human_detection_monitor.json
```

**期待される結果:**
- キャリブレーションが成功する
- 人間検出が動作する
- 座標変換が正しく動作する
- MotionPlannerへの変換が正常に動作する

**確認項目:**
- [ ] キャリブレーションが成功する
- [ ] 人間検出が動作する
- [ ] 座標変換が正しく動作する
- [ ] MotionPlannerへの変換が正常に動作する
- [ ] エンドツーエンドで動作する

### 8.4 統合テストチェックリスト

**人間検出システム:**
- [ ] 人間検出モニターが正常に起動する
- [ ] 人間が検出される
- [ ] トラッキングが動作する
- [ ] toio座標が表示される

**MotionPlanner統合:**
- [ ] DynamicObstacle変換が正常に動作する
- [ ] 未来位置の予測が含まれている
- [ ] 確信度が適切に設定されている

**エンドツーエンド:**
- [ ] キャリブレーションから人間検出まで動作する
- [ ] 座標変換が正しく動作する
- [ ] MotionPlannerへの変換が正常に動作する

---

## 9. フェーズ6: 回帰テスト

### 9.1 変更前後の比較テスト

#### テスト6-1: 変更前後の比較テスト

**実行コマンド:**
```bash
# 変更前のキャリブレーション結果を保存
cp calib_result.json calib_result_before.json

# 変更後のキャリブレーション実行
sudo ./build/capture_calibration \
  calibration_config_low_res.json \
  calib_result_after.json

# 結果の比較
diff <(cat calib_result_before.json | jq -S '.') <(cat calib_result_after.json | jq -S '.')
```

**期待される結果:**
- 変更前後で結果が一貫している
- 精度が低下していない

**確認項目:**
- [ ] 変更前後で結果が一貫している
- [ ] 精度が低下していない
- [ ] 新機能が正常に動作する

### 9.2 パフォーマンステスト

#### テスト6-2: 処理時間測定テスト

**実行コマンド:**
```bash
# キャリブレーション実行時間の測定
time sudo ./build/capture_calibration \
  calibration_config_low_res.json \
  calib_result.json
```

**期待される結果:**
- 処理時間が ≤ 200ms である（1回のキャリブレーション）
- 5回のキャリブレーションで ≤ 2 minutes である

**確認項目:**
- [ ] 処理時間が ≤ 200ms である
- [ ] 5回のキャリブレーションで ≤ 2 minutes である
- [ ] 処理時間が安定している

#### テスト6-3: フレームレート測定テスト

**実行コマンド:**
```bash
# インタラクティブツールでフレームレートを確認
sudo ./build/capture_calibration_interactive \
  calibration_config_low_res.json \
  calib_result.json
```

**期待される結果:**
- プレビューフレームレートが ≥ 10 FPS である（目標: 15 FPS）
- キャプチャレスポンスが ≤ 500ms である
- キャリブレーション処理が ≤ 5s である

**確認項目:**
- [ ] プレビューフレームレートが ≥ 10 FPS である
- [ ] キャプチャレスポンスが ≤ 500ms である
- [ ] キャリブレーション処理が ≤ 5s である

### 9.3 長時間実行テスト

#### テスト6-4: 長時間実行テスト

**実行コマンド:**
```bash
# 人間検出モニターを10分間実行
sudo ./build/monitor_human_detection \
  calib_result.json \
  config/human_detection_monitor.json
```

**期待される結果:**
- メモリリークがない
- 処理時間が安定している
- トラッキングが継続的に動作する

**確認項目:**
- [ ] メモリリークがない
- [ ] 処理時間が安定している
- [ ] トラッキングが継続的に動作する
- [ ] エラーが発生しない

### 9.4 回帰テストチェックリスト

**変更前後の比較:**
- [ ] 変更前後で結果が一貫している
- [ ] 精度が低下していない
- [ ] 新機能が正常に動作する

**パフォーマンス:**
- [ ] 処理時間が ≤ 200ms である
- [ ] フレームレートが ≥ 10 FPS である
- [ ] 処理時間が安定している

**長時間実行:**
- [ ] メモリリークがない
- [ ] 処理時間が安定している
- [ ] トラッキングが継続的に動作する

---

## 10. テスト結果記録

### 10.1 テスト結果テンプレート

```markdown
## テスト実行記録

**日付**: YYYY-MM-DD
**環境**: macOS 14.0 (Apple Silicon M2) / Linux (Ubuntu 20.04)
**カメラ**: Intel RealSense D415 (FW 5.16.0.1)
**USB接続**: USB 3.0 (直挿し)
**カメラ高さ**: 2400mm（実際の測定値、動作範囲: 2400-2800mm、推奨: 2500-2700mm）
**テスト担当者**: [名前]

### フェーズ0: 環境セットアップ

#### ハードウェアセットアップ
- [ ] RealSense D415が接続されている
- [ ] USB 3.0接続である
- [ ] カメラが2400-2800mmの高さに設置されている（推奨: 2500-2700mm）
- [ ] ChArUcoボードが印刷されている（実寸確認済み）
- [ ] toioプレイマットが平らに配置されている

#### ソフトウェアセットアップ
- [ ] 依存ライブラリがインストールされている
- [ ] ビルドが成功している
- [ ] 設定ファイルが準備されている

### フェーズ1: 初回セットアップテスト

| テストID | テスト項目 | 結果 | 備考 |
|---------|-----------|------|------|
| 1-1 | カメラ初期化 | PASS/FAIL |  |
| 1-2 | フレーム取得 | PASS/FAIL |  |
| 1-3 | ビルド結果確認 | PASS/FAIL |  |
| 1-4 | ヘルプ表示 | PASS/FAIL |  |
| 1-5 | 設定ファイル読み込み | PASS/FAIL |  |
| 1-6 | カメラ接続 | PASS/FAIL |  |

### フェーズ2: 基本機能テスト

| テストID | テスト項目 | 結果 | 備考 |
|---------|-----------|------|------|
| 2-1 | ChArUcoボード検出 | PASS/FAIL | 検出コーナー数: XX |
| 2-2 | 検出コーナー数 | PASS/FAIL | 平均: XX, 標準偏差: XX |
| 2-3 | 基本キャリブレーション実行 | PASS/FAIL |  |
| 2-4 | キャリブレーション結果検証 | PASS/FAIL |  |
| 2-5 | JSON出力 | PASS/FAIL |  |

**キャリブレーション結果:**
- 再投影誤差（ピクセル）: XX pixels
- 再投影誤差（ID units）: XX ID units
- 床面推定標準偏差: XX mm
- カメラ高さ: XX mm
- 変換誤差: XX ID units

### フェーズ3: 詳細機能テスト

| テストID | テスト項目 | 結果 | 備考 |
|---------|-----------|------|------|
| 3-1 | インタラクティブツール起動 | PASS/FAIL |  |
| 3-2 | ChArUco検出オーバーレイ | PASS/FAIL |  |
| 3-3 | キャプチャ実行 | PASS/FAIL |  |
| 3-4 | キーボードショートカット | PASS/FAIL |  |
| 3-5 | QCツール実行 | PASS/FAIL |  |
| 3-6 | 画像ピクセル → toio座標変換 | PASS/FAIL |  |
| 3-7 | toio座標 → 画像ピクセル変換 | PASS/FAIL |  |

### フェーズ4: 精度検証テスト

| テストID | テスト項目 | 結果 | 備考 |
|---------|-----------|------|------|
| 4-1 | 再投影誤差測定 | PASS/FAIL | 平均: XX, 標準偏差: XX |
| 4-2 | 床面推定精度検証 | PASS/FAIL | 標準偏差: XX mm |
| 4-3 | toio座標変換精度 | PASS/FAIL | 変換誤差: XX ID units |
| 4-4 | 既知点での精度検証 | PASS/FAIL | 誤差: XX ID units |
| 4-5 | 再現性テスト | PASS/FAIL | 標準偏差: XX |

### フェーズ5: 統合テスト

| テストID | テスト項目 | 結果 | 備考 |
|---------|-----------|------|------|
| 5-1 | 人間検出モニター起動 | PASS/FAIL |  |
| 5-2 | 人間検出動作 | PASS/FAIL |  |
| 5-3 | DynamicObstacle変換 | PASS/FAIL |  |
| 5-4 | エンドツーエンドテスト | PASS/FAIL |  |

### フェーズ6: 回帰テスト

| テストID | テスト項目 | 結果 | 備考 |
|---------|-----------|------|------|
| 6-1 | 変更前後の比較 | PASS/FAIL |  |
| 6-2 | 処理時間測定 | PASS/FAIL | 処理時間: XX ms |
| 6-3 | フレームレート測定 | PASS/FAIL | フレームレート: XX FPS |
| 6-4 | 長時間実行 | PASS/FAIL | 実行時間: XX min |

### 検出された問題

1. **問題1**: [問題の説明]
   - **影響**: [影響範囲]
   - **対処法**: [対処法]
   - **ステータス**: [OPEN/CLOSED]

2. **問題2**: [問題の説明]
   - **影響**: [影響範囲]
   - **対処法**: [対処法]
   - **ステータス**: [OPEN/CLOSED]

### 推奨アクション

1. [推奨アクション1]
2. [推奨アクション2]
3. [推奨アクション3]

### テスト結果サマリー

- **総テスト数**: XX
- **成功**: XX
- **失敗**: XX
- **スキップ**: XX
- **成功率**: XX%

### 次のステップ

1. [次のステップ1]
2. [次のステップ2]
3. [次のステップ3]
```

### 10.2 テスト結果の保存

**実行コマンド:**
```bash
# テスト結果ディレクトリの作成
mkdir -p test_results/$(date +%Y%m%d)

# テスト結果の保存
cp calib_result.json test_results/$(date +%Y%m%d)/
cp qc_report.md test_results/$(date +%Y%m%d)/
cp qc_report.json test_results/$(date +%Y%m%d)/

# テストログの保存
cp /tmp/test_output.log test_results/$(date +%Y%m%d)/ 2>/dev/null || true

# テスト結果テンプレートのコピー
cp docs/PHYSICAL_DEVICE_TESTING_FLOW.md test_results/$(date +%Y%m%d)/test_report_template.md
```

### 10.3 テスト結果の分析

**実行コマンド:**
```bash
# 再投影誤差の統計分析
echo "Reprojection Error Statistics:"
for file in test_results/*/calib_result.json; do
  if [ -f "$file" ]; then
    cat "$file" | jq -r '.quality_metrics.reprojection_error_px'
  fi
done | awk '{sum+=$1; sumsq+=$1*$1; count++} END {if(count>0) print "Mean:", sum/count, "StdDev:", sqrt(sumsq/count - (sum/count)^2), "Count:", count}'

# 床面推定の統計分析
echo "Floor Plane Statistics:"
for file in test_results/*/calib_result.json; do
  if [ -f "$file" ]; then
    cat "$file" | jq -r '.floor_plane.std_mm'
  fi
done | awk '{sum+=$1; sumsq+=$1*$1; count++} END {if(count>0) print "Mean:", sum/count, "StdDev:", sqrt(sumsq/count - (sum/count)^2), "Count:", count}'
```

---

## 11. トラブルシューティング

### 11.1 よくある問題と対処法

#### 問題1: カメラが検出されない

**症状:**
```
Failed to start RealSense pipeline: No device connected
```

**対処法:**
```bash
# macOS: カメラプロセスを停止
sudo killall VDCAssistant AppleCameraAssistant 2>/dev/null || true

# デバイスの確認
sudo /opt/homebrew/bin/rs-enumerate-devices

# USB接続の確認
system_profiler SPUSBDataType | grep -A 5 "Intel RealSense"

# カメラの抜き差し
# → USB-Cポートに直挿し（ハブ経由でない）
```

#### 問題2: ChArUcoボードが検出されない

**症状:**
```
ChArUco board not detected in current frame
```

**対処法:**
- ボードの位置を調整（カメラから40-80cmの距離）
- 照明条件を改善（均一な明るさ、グレアを避ける）
- ボードの傾きを確認（床面に対して平行）
- ボードのサイズを確認（45mm正方形、33mmマーカー）
- カメラの焦点距離を確認

#### 問題3: 床面推定が失敗する

**症状:**
```
Floor plane estimation failed
Floor plane std exceeds threshold
```

**対処法:**
- 深度データの品質を確認（ノイズ、外乱の有無）
- カメラ高さを確認（動作範囲: 2400-2800mm、推奨: 2500-2700mm）
  - 実際の測定例: 2400mm（240cm）でも動作可能
  - 2400-2500mmの範囲では警告が表示される可能性があるが、動作には問題なし
- 床面が平らで不透明であることを確認
- 設定パラメータを調整（必要に応じて）:
  - `floor_z_min_mm`: 2400.0（デフォルト、カメラ高さの下限）
  - `floor_z_max_mm`: 2800.0（デフォルト、カメラ高さの上限）
  - `floor_inlier_threshold_mm`: 8.0
  - `floor_ransac_iterations`: 500

#### 問題4: 再投影誤差が大きい

**症状:**
```
reprojection error exceeds threshold
```

**対処法:**
- ChArUcoボードの検出品質を改善（より多くのコーナーを検出）
- ボードの配置を調整（カメラに対して平行）
- サブピクセル補正が有効であることを確認
- ホモグラフィRANSAC閾値を調整（`homography_ransac_thresh_px`）

#### 問題5: toio座標変換の誤差が大きい

**症状:**
```
transform_error_at_correspondences exceeds threshold
```

**対処法:**
- toio_playmat.json の対応点を確認・更新
- ChArUcoボードの物理的な配置位置を確認
- ボードマウントラベルが正しいことを確認（`board_mount_label`）
- 対応点の数を増やす（3点以上推奨）

#### 問題6: インタラクティブツールで「Frame didn't arrive within 15000」エラーが発生する

**症状:**
```
[error] RealSense capture error: Frame didn't arrive within 15000
```

このエラーが**1回だけ発生する場合**と**継続的に15秒ごとに発生する場合**で対処法が異なります。

##### 6-1. エラーが1回だけ発生する場合（一時的な問題）

**原因:**
- ウィンドウを全画面にした場合に発生しやすい
- 一時的なUSB接続の不安定さ
- カメラプロセスとの競合

**対処法:**
1. **ウィンドウサイズの調整**（最も効果的）:
   - ウィンドウを全画面にしない
   - ウィンドウサイズを適切なサイズ（例: 1280×720、1920×1080）に保つ
   - 全画面表示が必要な場合は、ウィンドウをリサイズして使用

2. **カメラプロセスの停止**:
   ```bash
   sudo killall VDCAssistant AppleCameraAssistant 2>/dev/null || true
   ```

3. **ツールの再起動**:
   - ツールを終了して再起動

##### 6-2. エラーが継続的に15秒ごとに発生する場合（重大な問題）

**症状A: パイプライン開始成功だがフレーム取得失敗（最も一般的）**
```
[2025-11-09 14:47:17.885] [info] RealSense pipeline started for calibration
[2025-11-09 14:47:17.912] [info] Loaded color intrinsics fx=613.01, fy=612.75, cx=326.75, cy=246.01
[2025-11-09 14:47:17.914] [info] Enabled auto-exposure on color sensor
[2025-11-09 14:47:17.914] [info] Depth scale: 0.001000 meters per unit
[2025-11-09 14:47:33.004] [error] RealSense capture error: Frame didn't arrive within 15000
[2025-11-09 14:47:48.024] [error] RealSense capture error: Frame didn't arrive within 15000
...
```
（パイプラインは正常に開始されているが、フレームが取得できない）

**症状B: パイプライン開始前からエラーが発生**
```
[error] RealSense capture error: Frame didn't arrive within 15000
[error] RealSense capture error: Frame didn't arrive within 15000
...
```
（パイプライン開始のログが表示されない）

**原因:**
- **症状A（パイプライン開始成功）の場合**:
  - USB接続の問題（USB 2.0で動作している、帯域幅不足）
  - カメラがフレームを送信していない（ハードウェア的な問題）
  - カメラプロセスとの競合が残っている
  - 設定（解像度、フレームレート）が高すぎてUSB 2.0では対応できない
- **症状B（パイプライン開始失敗）の場合**:
  - カメラが検出されていない
  - USB接続の問題
  - カメラの故障

**診断手順（段階的に実行）:**

> **重要**: 症状A（パイプライン開始成功）の場合、**USB 2.0で動作している可能性が非常に高い**です。まずステップ2（USB接続速度の確認）を最優先で確認してください。

**ステップ1: インタラクティブツールの終了とカメラプロセスの停止**
```bash
# 1. インタラクティブツールを終了（Ctrl+C または 'q'キー）

# 2. すべてのカメラ関連プロセスを停止
sudo killall VDCAssistant AppleCameraAssistant 2>/dev/null || true
sudo killall capture_calibration_interactive 2>/dev/null || true

# 3. 少し待機（2-3秒）
sleep 3
```

**ステップ2: USB接続速度の確認（最優先）**
```bash
# USB接続速度の確認
system_profiler SPUSBDataType | grep -i -B 2 -A 10 "0AD3\|realsense"

# 出力例（USB 2.0の場合）:
#   Product ID: 0x0ad3
#   Speed: Up to 480 Mb/s    ← USB 2.0（問題の原因）
#
# 出力例（USB 3.0の場合）:
#   Product ID: 0x0ad3
#   Speed: Up to 5 Gb/s      ← USB 3.0（正常）

# USB 2.0で動作している場合の対処法:
# 1. Mac本体のUSB-Cポートに直挿し（ハブ経由でない）
# 2. USB 3.0対応のケーブルを使用（USB-C to USB-C、またはUSB-C to USB-A 3.0）
# 3. 別のUSB-Cポートを試す
# 4. ケーブルを交換
```

**ステップ3: カメラの検出確認**
```bash
# デバイスの確認（Rosetta2経由で実行する場合）
arch -x86_64 sudo /opt/homebrew/bin/rs-enumerate-devices

# カメラが検出されない場合:
# → USB接続を確認、カメラを抜き差し
# → ステップ2に戻る
```

**ステップ4: カメラの動作確認（rs-capture）**
```bash
# RealSense SDKの標準ツールでカメラが動作するか確認
# 重要: インタラクティブツールを終了してから実行
arch -x86_64 sudo /opt/homebrew/bin/rs-capture

# rs-captureでもフレームが取得できない場合:
# → USB接続の問題、カメラの故障の可能性
# → ステップ2（USB接続速度）を再確認
# → カメラを抜き差しして再試行
# → それでも解決しない場合、ステップ6に進む

# rs-captureでフレームが取得できる場合:
# → インタラクティブツール側の問題
# → ステップ5に進む
```

**ステップ5: インタラクティブツールの再起動**
```bash
# 1. カメラを抜き差し（5秒待機）
# （物理的にUSBケーブルを抜いて、5秒待ってから再度接続）
# 重要: USB 3.0ポートに直挿し

# 2. カメラプロセスを停止
sudo killall VDCAssistant AppleCameraAssistant 2>/dev/null || true

# 3. カメラの検出を確認
arch -x86_64 sudo /opt/homebrew/bin/rs-enumerate-devices

# 4. USB接続速度を再確認（USB 3.0であることを確認）
system_profiler SPUSBDataType | grep -i -B 2 -A 10 "0AD3\|realsense"

# 5. インタラクティブツールを再起動（正しいディレクトリから）
cd /Users/ksk432/technetope/locomotion/calibration
arch -x86_64 sudo ./build/capture_calibration_interactive \
  calibration_config_low_res.json \
  /tmp/test_calib_result.json

# 6. ウィンドウを全画面にしない（適切なサイズに保つ）
```

**ステップ6: USB 2.0で動作させる場合（USB 3.0が利用できない場合）**

USB 2.0しか持っていない場合、フレームレートを下げる必要があります。USB 2.0の帯域幅（480 Mb/s）では、640x480@15fpsのカラーと深度ストリームを同時に送信するのは帯域幅不足です。

**対処法: USB 2.0用の設定ファイルを使用**

1. **USB 2.0用の設定ファイルを作成**（既に作成済み: `calibration_config_usb2.json`）:
   ```json
   {
     "fps": 6,  // 15fps → 6fpsに下げる
     "color_width": 640,
     "color_height": 480,
     "depth_width": 640,
     "depth_height": 480,
     ...
   }
   ```

2. **USB 2.0用設定ファイルでインタラクティブツールを起動**:
   ```bash
   cd /Users/ksk432/technetope/locomotion/calibration
   arch -x86_64 sudo ./build/capture_calibration_interactive \
     calibration_config_usb2.json \
     /tmp/test_calib_result.json
   ```

3. **動作確認**:
   - フレームレートが6fpsになるため、表示が少し遅く感じる可能性があります
   - ただし、フレーム取得のタイムアウトエラーは解消されるはずです
   - ChArUco検出やキャリブレーションの精度には影響しません（解像度は同じ640x480）

**注意事項:**
- USB 2.0では最大6fpsが推奨（RealSense D415がサポートする最低フレームレート）
- 15fpsや30fpsはUSB 2.0の帯域幅では対応できません
- 将来的にUSB 3.0が利用可能になったら、`calibration_config_low_res.json`（15fps）または`calibration_config.json`（15fps）を使用してください

**ステップ7: それでも解決しない場合**
```bash
# 1. システムの再起動
sudo reboot

# 2. 再起動後、再度ステップ1から確認
# 3. USB 2.0用設定ファイル（calibration_config_usb2.json）を使用
# 4. カメラのハードウェア的な故障の可能性を検討
```

**予防策:**
- インタラクティブツールを使用する際は、ウィンドウを全画面にしない
- ウィンドウサイズを適切なサイズに保つ
- カメラプロセスを停止してからツールを起動
- **USB 2.0の場合**: `calibration_config_usb2.json`（6fps）を使用
- **USB 3.0の場合**: `calibration_config_low_res.json`（15fps）または`calibration_config.json`（15fps）を使用
- Mac本体のUSB-Cポートに直挿し（ハブ経由でない）

### 11.2 ログレベルの調整

**デバッグ時に詳細なログを出力する場合:**
```json
{
  "log_level": "debug"
}
```

**または、環境変数で設定:**
```bash
export SPDLOG_LEVEL=debug
sudo ./build/capture_calibration ...
```

### 11.3 デバッグフレームの保存

**インタラクティブツールで `S` キーを押すと、デバッグフレームが保存されます:**
```bash
# デバッグフレームの確認
ls -la logs/calibration_debug/

# フレームの確認
# （カラー画像と深度画像がPNG形式で保存される）
```

---

## 12. 参考資料

- [TESTING.md](../TESTING.md) - 実機テスト手順書（既存）
- [README.md](../README.md) - モジュールの概要
- [REQUIREMENTS_CALIBRATION_V2.md](../REQUIREMENTS_CALIBRATION_V2.md) - 要件仕様
- [STATUS.md](../STATUS.md) - 実装状況
- [APPLE_SILICON_COMPATIBILITY.md](../APPLE_SILICON_COMPATIBILITY.md) - Apple Silicon環境での注意事項
- [docs/four_point_measurement.md](four_point_measurement.md) - 四点計測手順
- [docs/PRINTING_GUIDE.md](PRINTING_GUIDE.md) - 印刷ガイド
- [HUMAN_DETECTION_VERIFICATION.md](../HUMAN_DETECTION_VERIFICATION.md) - 人間検出システム検証手順

---

## 13. 更新履歴

- 2025-11-09: 初版作成

---

**最終更新**: 2025-11-09  
**作成者**: AI Assistant  
**レビュー状況**: DRAFT - 実機テスト後に更新予定

