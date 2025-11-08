# Calibration Module - 実装状況サマリー

**最終更新:** 2025-11-08

このドキュメントは、キャリブレーションモジュールの実装状況を一目で把握するためのサマリーです。

---

## 📊 進捗状況（全体: 85%完了）

```
███████████████████░░ 85%
```

### コンポーネント別進捗

| コンポーネント | 進捗 | 状態 | 備考 |
|---------------|------|------|------|
| CharucoDetector | 100% | ✅ 完了 | 公開API、サブピクセル補正 |
| PlaymatLayout | 100% | ✅ 完了 | JSON読み込み、アフィン変換 |
| CalibrationSession | 98% | ✅ ほぼ完了 | JSON v2.0 + validation 出力済 |
| CalibrationPipeline | 90% | 🟢 完了 | Intrinsics取得＋歪み補正＋床面推定実装 |
| FloorPlaneEstimator | 95% | 🟢 完了 | RANSAC + 3D点群フィルタリング対応 |
| QC Scripts | 0% | ⬜ 未着手 | - |

---

## ✅ 完了した重点タスク

### 1. CalibrationPipeline: Intrinsics＋歪み補正
- RealSense の `rs2_intrinsics` を `CameraIntrinsics` に写像し、`camera_matrix_`/`dist_coeffs_` を保持（`src/CalibrationPipeline.cpp`）
- `captureAlignedFrame()` で `cv::undistort` を適用し、歪み補正済みフレームで Charuco 検出
- Intrinsics と歪みモデルを `CalibrationSnapshot` → `CalibrationResult` → JSON v2.0 へシリアライズ

### 2. FloorPlaneEstimator / Config 拡張
- `CalibrationConfig` に RANSAC 関連・高さレンジ・ダウンサンプル・`random_seed` を追加し CLI から読込可能に
- `FloorPlaneEstimator` が intrinsics + depth scale から 3D 点群を生成、範囲フィルタ＋RANSAC＋SVD 再フィットで平面推定
- 標準偏差とインライヤ率を返し、`CalibrationPipeline` で閾値判定を行えるようにした

### 3. CalibrationSession / JSON v2.0 対応
- `CalibrationSnapshot` に Intrinsics を保持し、`CalibrationSession::SaveResultJson()` がスキーマ v2.0（intrinsics, floor_plane, validation）で保存
- Validation セクションで再投影誤差 / 床面 std / インライヤ比の PASS/FAIL を記録し、QC 手順と連動

---

## 🟡 次の優先タスク（2項目）

### 1. QC スクリプト & 自動レポート
- `tools/run_calibration_qc.{cpp,py}` を作成し、RealSense 接続〜スナップショット取得〜判定を自動化
- `ctest -L qc` から呼び出せるよう CMake に統合、Markdown/JSON レポートのテンプレ追加

### 2. 統合テストとデータ収集
- Charuco モックデータ／深度点群のフィクスチャを用意し、`test_floor_plane_estimator.cpp` 等を仕上げる
- 実機リトライ統計（成功率・平均誤差）を `SessionConfig::save_intermediate_snapshots` を活用して収集

---

## 🟢 その他のタスク

- QC スクリプト実装（上記）
- `tools/run_calibration_qc.cpp` or `.py`
- CMake ctest統合

---

## 📝 関連ドキュメント

| ドキュメント | 内容 |
|-------------|------|
| [workingflow.md](../../workingflow.md) | 実装の優先順位、タイムライン、QCフロー |
| [implementation_requirements.md](implementation_requirements.md) | 詳細な技術仕様とクラス設計 |
| [docs/apple_silicon_realsense.md](../../docs/apple_silicon_realsense.md) | Apple Silicon環境での注意事項 |

---

## 🍎 Apple Silicon環境での開発

**必須手順:**
```bash
# 毎回実行
sudo killall VDCAssistant AppleCameraAssistant 2>/dev/null || true
# カメラ抜き差し後、即座に実行
sudo ./capture_calibration
```

**主な問題:**
- sudo実行が必須
- USB3接続推奨（USB2は15 FPS制限）
- macOSカメラプロセスとの競合

---

## 🧪 検証コマンド

```bash
# ビルド
cmake -B build -DLOCOMOTION_BUILD_TESTS=ON
cmake --build build

# モックテスト（sudo不要）
cd build && ctest

# 実機テスト（Apple Silicon、sudo必須）
sudo killall VDCAssistant AppleCameraAssistant 2>/dev/null || true
sudo ./build/capture_calibration
```

---

## 📅 次のアクション

1. ✅ ドキュメント整備 → **完了**
2. ⬜ **CalibrationPipelineにIntrinsics処理を追加** → **次の最優先タスク**
3. ⬜ CalibrationConfigの拡張
4. ⬜ FloorPlaneEstimatorの拡張
5. ⬜ 統合テストで全フロー検証
6. ⬜ QCスクリプト実装

---

**更新履歴:**
- 2025-01-08: 初版作成、実装状況の整理完了
