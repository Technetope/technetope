# Swarm Control コードレビュー結果

## 概要

`swarm_control`を厳しく精査し、acousticsとの統合を考慮したレビューを実施しました。

## 発見された問題点

### 1. 暗号化の統合が不完全 ⚠️ **重要**

**問題**: `OscReceiver`と`OscSender`に暗号化メソッドがあるが、実際には使用されていない。

#### OscReceiver (`osc_receiver.cpp`)
- `enableEncryption()`はキーとIVを保存するが、`OscListener`には渡されていない
- `acoustics::osc::OscListener`は暗号化サポートを持たないため、受信時に復号されない
- `encryptionEnabled_`フラグは存在するが使用されていない

#### OscSender (`osc_sender.cpp`)
- 暗号化メソッドが公開されていない
- `acoustics::osc::OscSender`には`enableEncryption()`があるが、swarm_controlから呼び出されていない
- `main.cpp`で設定ファイルから暗号化設定を読み込んでいるが、実際には使われていない（TODOコメントあり）

**影響**: 暗号化を有効にしても実際には暗号化されず、acousticsと統一された通信ができない。

**推奨修正**:
```cpp
// OscSenderに暗号化メソッドを追加
void OscSender::enableEncryption(const acoustics::osc::OscEncryptor::Key256& key,
                                 const acoustics::osc::OscEncryptor::Iv128& iv) {
    if (sender_) {
        sender_->enableEncryption(key, iv);
    }
}

// main.cppで実際に使用
if (config.osc.encryptionEnabled && config.osc.encryptionKeyFile.has_value()) {
    // キーファイルを読み込んで設定
    // oscSender.enableEncryption(key, iv);
    // oscReceiver.enableEncryption(key, iv);
}
```

**注意**: `OscListener`は暗号化サポートがないため、受信側の暗号化は別の層で実装する必要がある。

### 2. デバイスインデックスの再マッピング問題 ⚠️ **中程度**

**問題**: `DeviceManager::removeDevice()`でデバイスを削除すると、全インデックスが再割り当てされる。

```cpp:device_manager.cpp
// インデックスを再マッピング
indexToDeviceId_.clear();
int newIndex = 0;
for (auto& [id, assignment] : assignments_) {
    assignment.assignedIndex = newIndex;  // 既存のインデックスが変更される
    indexToDeviceId_[newIndex] = id;
    newIndex++;
}
```

**影響**: 
- ロボットが既に`RobotAgent`を作成済みの場合、インデックスが突然変わる
- OSC送信先アドレス（`/toio/{robotIndex}/target`）が変わる可能性
- 既存のロボットリストとの不整合が発生する可能性

**推奨修正**: 
- インデックスは削除時に空き番号として残す（再利用可能にする）
- または、削除時にロボットリストも同期して更新する
- または、インデックスを変更せず、削除済みフラグで管理する

### 3. 時刻同期の問題（heartbeat処理）

**問題**: `osc_receiver.cpp:220-224`でheartbeatのレイテンシ計算が不正確。

```cpp
auto now = std::chrono::system_clock::now();
auto nowSeconds = std::chrono::duration_cast<std::chrono::seconds>(
    now.time_since_epoch()).count();
double latencyMs = (static_cast<double>(nowSeconds) - static_cast<double>(seconds)) * 1000.0;
```

**問題点**:
- ミリ秒単位の精度が必要だが、秒単位で計算している
- `micros`パラメータが使用されていない
- 時刻同期が正確でない可能性

**推奨修正**: NTP同期を前提とした正確な時刻計算を実装。

### 4. エラーハンドリングの一貫性

**問題**: 一部のエラーが警告のみで、処理が継続される。

**例**:
- `osc_sender.cpp`: 送信失敗時に警告のみ
- `device_manager.cpp`: JSON読み込み失敗時に警告のみ

**影響**: エラーが発生しても気づきにくい。

**推奨**: 致命的なエラーは例外を投げるか、状態フラグで管理する。

### 5. 設定ファイルのパス解決

**問題**: `config.cpp`で相対パスが使用されているが、実行ディレクトリに依存する。

```cpp
Config loadConfig(const std::filesystem::path& configPath = "config/swarm_config.json");
```

**推奨**: 実行ファイルのディレクトリを基準にするか、絶対パスをサポート。

## 良い点

### 1. アーキテクチャの統一性
- `acoustics::osc`ライブラリを適切に使用している
- `acoustics::common::DeviceRegistry`を統合している
- 名前空間の使用が適切

### 2. スレッドセーフティ
- `std::mutex`で適切に保護されている
- `std::lock_guard`が正しく使用されている

### 3. メモリ管理
- `std::unique_ptr`で適切に管理されている
- 生ポインタの使用が最小限

### 4. ログ出力
- `spdlog`を適切に使用
- デバッグ情報が充実

## Acousticsとの統合に関する確認事項

### OSCメッセージ形式
✅ `/announce`: 形式が一致している
✅ `/heartbeat`: 形式が一致している（レイテンシ計算は改善が必要）
✅ `/toio/position`: 形式が一致している
✅ `/toio/{robotIndex}/target`: 送信形式が適切

### DeviceRegistry統合
✅ `acoustics::common::DeviceRegistry`を正しく使用
✅ 永続化パスが適切に設定可能

### 通信プロトコル
⚠️ 暗号化が未実装（上記問題1を参照）
⚠️ 暗号化が実装されれば、acousticsと同じプロトコルで通信可能

## 推奨される修正の優先順位

### 高優先度
1. **暗号化の実装** - acousticsとの統合に必要
2. **デバイス削除時のインデックス再マッピング** - ランタイムエラーの原因になる可能性

### 中優先度
3. **heartbeatの時刻同期精度** - 監視の正確性向上
4. **設定ファイルのパス解決** - 実行環境の柔軟性向上

### 低優先度
5. **エラーハンドリングの一貫性** - コード品質の向上

## 結論

全体的にコード品質は高く、acousticsとの統合への準備も整っています。主な問題は暗号化の実装とデバイス管理の一部です。これらを修正すれば、acousticsと同じ規格で通信できるようになります。

