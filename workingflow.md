# Calibration Workflow Blueprint

本ドキュメントは、RealSense D415 と toio プレイマット環境におけるキャリブレーション／検証フローを共有するためのワークフロー定義です。開発者（Codex）と QC 担当（ユーザー）が同じ手順で動作確認できるよう、依存リポジトリ、実装ステップ、検証タスクを整理しています。

---

## 1. 参照コード（git subtree）

`external_references/` 以下にサブツリーとして取り込んだ OSS を活用します。

| Path | Upstream | 用途 |
| ---- | -------- | ---- |
| `external_references/Perception_Using_PCD` | https://github.com/ysm10801/Perception_Using_PCD | RealSense 点群生成＋RANSAC 実装の参考 |
| `external_references/PointCloud-Playground` | https://github.com/Loozin031/PointCloud-Playground | Open3D を用いた平面検出パラメータの参考 |
| `external_references/charuco_calibration` | https://github.com/CopterExpress/charuco_calibration | Charuco ベースの単体カメラキャリブレーション手順 |
| `external_references/stereo-calib` | https://github.com/shubhamwagh/stereo-calib | Charuco でのホモグラフィ／保存形式の参考 |

更新が必要な場合は `git subtree pull --prefix=... <repo> <branch> --squash` を実行してください。

---

## 2. 実装タスク

### 2.1 Intrinsics キャリブレーションツール
- 目的: D415 の内部パラメータ (`K`, `distCoeffs`) を Charuco ボードで再推定し、JSON 出力する。
- 参考:
  - `external_references/charuco_calibration/src/calibrate_camera.cpp`（OpenCV `calibrateCameraCharuco` 呼び出し部分を流用）
  - librealsense `examples/capture/rs-capture.cpp`（RealSense 接続とフレーム取得）
- 既存コードの再利用:
  - RealSense 初期化とフレーム取得は librealsense サンプルをベースにし、必要な部分のみコピー（`rs2::pipeline`, `rs2::config`）。
  - Charuco コーナー収集ループとキャリブ処理は `charuco_calibration` を縮小転用。
- 自作が必要な部分:
  - CLI オプション処理（キャプチャ枚数、出力パス）。
  - キャプチャ中のリアルタイム可視化（必要であれば OpenCV `imshow`）。
  - 出力 JSON（`nlohmann::json` を使用し、`K`, `distCoeffs`, `image_size`, 使用 Charuco 設定を保存）。
- 成果物:
  - `tools/calibrate_intrinsics.cpp`（新規）
  - `calibration/intrinsics/d415_intrinsics.json`
- 実装注意:
  - Charuco ボード仕様は `CalibrationConfig` と揃える。
  - 最低 15 枚程度のフレームを蓄積し、RMS 誤差と再投影誤差をログに出力。

### 2.2 `FloorPlaneEstimator` 実装
- 目的: 深度フレームから床平面を RANSAC で推定し、法線/距離/インライヤ統計を返す。
- 参考:
  - librealsense `examples/pointcloud/rs-pointcloud.cpp`（`rs2::pointcloud` の使い方）
  - `external_references/Perception_Using_PCD/src/segmentation/plane_segmentation.cpp`（RANSAC ロジック）
  - `external_references/PointCloud-Playground/scripts/plane_detection.py`（パラメータ設定の参考値）
- 既存コードの再利用:
  - RealSense から点群を得る処理は librealsense サンプルに準拠。
  - RANSAC の主要ロジック（サンプリング、距離評価）は `Perception_Using_PCD` をC++20スタイルに移植。
- 自作が必要な部分:
  - `FloorPlaneEstimator` クラス内でバッファ再利用や OpenMP を使わずに完結させる実装。
  - 点群ダウンサンプリング＆範囲フィルタ（テーブルなどの外乱を除去）。
  - 結果を `cv::Vec4f plane`, `plane_std_mm`, `inlier_ratio` に整形。
- 手順:
  1. `rs2::pointcloud` で XYZ 点群生成（必要に応じて `rs2::decimation_filter` 等のフィルタも適用）。
  2. ランダムサンプリング（3点）→平面候補→距離閾値でインライヤ算出。
  3. ベストモデル選択後、インライヤ集合で再フィット＆標準偏差計算。
- 出力: `FloorPlaneEstimate`（plane, plane_std_mm, inlier_ratio）
- パラメータ: `FloorPlaneEstimatorConfig` で閾値、反復数、最小サンプル数を制御

### 2.3 `CalibrationPipeline` 拡張
- Intrinsics 読み込み
  - `CalibrationConfig` に `intrinsics_path` を追加し、起動時に JSON をロード
  - カラー画像は常に歪み補正してから Charuco 検出へ渡す
- ホモグラフィ計算
  - 現行 `findHomography` フローを維持しつつ、結果を `calib_result.json` に保存
  - 新たに intrinsics / distCoeffs / floor plane を同ファイルへ統合
- FloorPlaneEstimator 連携
  - 推定結果の標準偏差・インライヤ比をセッション閾値で判定
- 既存コードの再利用:
  - 歪み補正は OpenCV の `initUndistortRectifyMap` / `remap` を使用。
  - JSON 書き込みは既存の `CalibrationSession::SaveResultJson` を拡張。
- 自作が必要な部分:
  - Intrinsics JSON のフィールド定義（`fx`, `fy`, `cx`, `cy`, `k1...k5` など）。
  - `CalibrationConfig` 拡張とバリデーション。
  - セッションログに intrinsics / floor plane の統計を追加。

### 2.4 QC 用スクリプト
- 目的: Intrinsics → Charuco → ホモグラフィ → 床面 推定を一括検証
- 例: `tools/run_calibration_qc.sh`
  1. Intrinsics JSON の読み込みテスト
  2. Charuco検出（再投影誤差が閾値以下か確認）
  3. ホモグラフィ適用後のランドマーク誤差を数値化
  4. 深度フレームで床面推定→残差・インライヤ率を表示
- スクリプト出力を QA 担当が確認し、パス/フェイルを判断
- 既存コードの再利用:
  - librealsense CLI サンプルから RealSense 接続確認ロジックを流用。
  - `CalibrationPipeline` 実行部をラップして再利用。
- 自作が必要な部分:
  - Bash/Python スクリプトによる自動閾値判定。
  - QC ログフォーマット（JSON or Markdown）生成。

---

## 3. QC フロー

| フェーズ | デベロッパ作業 | QC 担当作業 |
| -------- | --------------- | ----------- |
| Intrinsics | サンプルデータでツール動作を確認し、RMS/再投影誤差を提示 | 実機データでツールを実行し、JSON とログをレビュー |
| Floor plane | サンプル深度で平面係数・標準偏差を共有 | 実際の床環境で `run_calibration_qc.sh` を実行、閾値超過がないか確認 |
| Homography | `CalibrationPipeline` のログ（Charuco検出数、誤差）を共有 | toio プレイマットで試験実行し、結果 JSON をレビュー |
| 回帰 | 変更が入った箇所のユニット/統合テストを再実行 | 最新コードで QC スクリプトを再走して合否を判断 |

---

## 4. 今後のタスク整理

1. Intrinsics ツール実装・レビュー
2. `FloorPlaneEstimator` RANSAC 実装・テスト
3. `CalibrationPipeline` の歪み補正対応と結果フォーマット更新
4. QC スクリプト整備と文書化
5. 深度クラスタリング（足/頭検出）は上記完了後に着手

本ワークフローに沿って実装と QC を往復することで、座標系統一と床検出を堅実に進められます。追加の要望や変更があれば、このファイルを更新して合意形成に使ってください。
