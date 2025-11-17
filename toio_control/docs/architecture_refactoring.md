# アーキテクチャ整理とリファクタリング

このドキュメントは、toio_controlプロジェクトのコードベース整理とリファクタリングの内容を記録します。

## 実施日
2025年11月

## 整理の目的

1. **通信レイヤーとアルゴリズムの分離**: コードの責務を明確化
2. **機能別ディレクトリ構造**: 保守性と可読性の向上
3. **用途別OSC送信実装**: scheduler（音響制御）とswarm_control（toio制御）の最適化

---

## 1. swarm_control のディレクトリ構造整理

### 1.1 整理前の構造
```
src/
├── *.cpp, *.hpp (すべてフラット)
```

### 1.2 整理後の構造
```
src/
├── algorithm/          # アルゴリズム・モデル
│   ├── agent/         # ロボットエージェント本体
│   │   └── robot_agent.{cpp,hpp}
│   ├── collision/     # 衝突回避系
│   │   ├── predictive_avoidance.{cpp,hpp}
│   │   └── urgent_escape.{cpp,hpp}
│   ├── flocking/      # 群れ行動系
│   │   ├── boid_model.{cpp,hpp}
│   │   └── cluster_detector.{cpp,hpp}
│   └── spatial/       # 空間解析系
│       ├── spatial_density_grid.{cpp,hpp}
│       └── spot_potential.{cpp,hpp}
├── comm/              # 通信レイヤー（OSC送受信）
│   ├── osc_sender.{cpp,hpp}
│   └── osc_receiver.{cpp,hpp}
├── config/            # 設定
│   ├── config.{cpp,hpp}
│   └── params.{cpp,hpp}
├── utils/             # ユーティリティ
│   ├── device/        # デバイス管理
│   │   └── device_manager.{cpp,hpp}
│   ├── types/         # 型定義
│   │   ├── human_spot.hpp
│   │   ├── position.hpp
│   │   └── velocity.hpp
│   └── utils.{cpp,hpp} # ユーティリティ関数
└── main.cpp
```

### 1.3 整理の効果
- **責務の明確化**: 通信、アルゴリズム、設定、ユーティリティが明確に分離
- **保守性向上**: 関連ファイルが同じディレクトリに集約
- **可読性向上**: ディレクトリ構造から機能が推測可能

---

## 2. scheduler のディレクトリ構造整理

### 2.1 整理前の構造
```
src/
├── SchedulerController.cpp
├── SoundTimeline.cpp
└── TargetResolver.cpp
```

### 2.2 整理後の構造
```
src/
├── audio/             # 音響タイムライン関連
│   └── SoundTimeline.cpp
├── osc/               # OSC送信関連（最適化済み）
│   └── osc_bundle_sender.cpp
├── config/            # 設定・ターゲット解決関連
│   └── TargetResolver.cpp
└── SchedulerController.cpp
```

### 2.3 名前空間の整理
- `toio_control::scheduler::audio` - 音響関連
- `toio_control::scheduler::osc` - OSC送信関連
- `toio_control::scheduler::config` - 設定関連

---

## 3. OSC送信実装の最適化と分離

### 3.1 問題点
- schedulerとswarm_controlでOSC送信の用途が異なるのに、同じ実装パターンを使用
- schedulerは毎回新しい`io_context`を作成（非効率）

### 3.2 解決策

#### scheduler（音響制御）: `OscBundleSender`
- **用途**: 低頻度のバッチ送信（タイムラインに基づく）
- **最適化**:
  - `IoContextRunner`を使用して接続を再利用
  - 送信完了後に明示的に切断可能（`disconnect()`メソッド）
  - リソースを早期解放
- **特徴**:
  - バッチ送信に特化
  - 送信間隔制御（`bundleSpacing`）

#### swarm_control（toioドライブ制御）: `OscSender`
- **用途**: 高頻度の連続送信（1秒ごとなど）
- **最適化**:
  - 常時接続を維持（接続の確立・切断のオーバーヘッドを回避）
  - リアルタイム制御に最適化
- **特徴**:
  - 複数ロボットの目標座標をバンドルで一括送信
  - 接続状態の管理

### 3.3 実装の違い

| 項目 | scheduler (音響制御) | swarm_control (toio制御) |
|------|---------------------|-------------------------|
| **送信頻度** | 低頻度（バッチ） | 高頻度（1秒ごと） |
| **接続管理** | 送信後切断 | 常時接続維持 |
| **最適化** | リソース効率 | レイテンシ最小化 |
| **用途** | タイムライン送信 | リアルタイム制御 |
| **クラス名** | `OscBundleSender` | `OscSender` |
| **名前空間** | `toio_control::scheduler::osc` | `swarm_control` |

---

## 4. 変更ファイル一覧

### 4.1 swarm_control
- **ディレクトリ移動**: 全ファイルを機能別ディレクトリに移動
- **includeパス更新**: すべてのincludeパスを新しい構造に合わせて更新
- **CMakeLists.txt**: 新しいディレクトリ構造に対応

### 4.2 scheduler
- **ディレクトリ移動**: 機能別ディレクトリに移動
- **新規作成**: `osc/osc_bundle_sender.{cpp,hpp}` - 最適化されたOSC送信実装
- **名前空間更新**: すべての名前空間を更新
- **CMakeLists.txt**: 新しいディレクトリ構造に対応
- **テストファイル**: includeパスと名前空間を更新

---

## 5. ベストプラクティス

### 5.1 ディレクトリ構造の原則
1. **機能別に分離**: 通信、アルゴリズム、設定、ユーティリティを明確に分離
2. **階層を適切に**: 深すぎず、浅すぎない（2-3階層が目安）
3. **命名規則**: ディレクトリ名は機能を明確に示す

### 5.2 OSC送信実装の選択
- **高頻度送信**: `swarm_control::OscSender`（常時接続）
- **低頻度バッチ送信**: `toio_control::scheduler::osc::OscBundleSender`（送信後切断）

### 5.3 名前空間の使用
- 機能ごとに名前空間を分離
- ネストした名前空間で階層を表現（例: `toio_control::scheduler::audio`）

---

## 6. 今後の拡張

### 6.1 検討事項
- 共通OSC送信ライブラリの抽出（現状は用途が異なるため分離を維持）
- さらなる最適化（バッファリング、非同期送信など）

### 6.2 注意事項
- schedulerとswarm_controlのOSC送信実装は用途が異なるため、統合しない
- 各実装はそれぞれの用途に最適化されている

---

## 7. 参考資料

- [OSC実装ドキュメント](./osc_implementation.md)
- [OSC契約仕様](./osc_contract.md)
- [swarm_control README](../pc_tools/swarm_control/README.md)
- [scheduler README](../pc_tools/scheduler/README.md)

