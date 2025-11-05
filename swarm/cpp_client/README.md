# Toio C++ Client

シンプルな CLI から Toio 中継サーバーへ WebSocket で接続し、コマンド／クエリ／通知購読を行うクライアントです。`DESIGN.md` にある方針をそのまま形にした実装になっています。

## 必要要件
- C++20 対応コンパイラ (clang++ 15 以降推奨)
- CMake 3.20+
- Boost 1.78+ (`system`, `thread` コンポーネント)
- `nlohmann_json` (システムに無い場合は CMake の `FetchContent` で自動取得します)

## ビルド手順
```bash
cd swarm/cpp_client
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### 実行例
```bash
./build/toio_cli --id F3H --host 127.0.0.1 --port 8765 --subscribe
```

起動後は以下のようなコマンドを入力できます。
```
help            # コマンド一覧
move -30 30 0   # 左右モーター出力、末尾0でresultレス省略
led 255 0 0     # LED を赤に
battery         # 電池クエリ
subscribe       # 位置購読開始（notify true）
unsubscribe     # 購読解除
exit            # 終了
```

受信した `result` / `response` / `system` / `error` は JSON 文字列として標準出力に表示されます。
