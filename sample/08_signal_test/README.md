# 08_signal_test

終了シグナル（SIGINT, SIGTERM）受信時のクリーンアップ（メモリリークなし、FDクローズ）を確認するテストです。

## 概要

`valgrind` を使用してサーバーを起動し、外部からシグナルを送って終了させ、以下の点を確認します。

1. 正常終了すること（Exit Code 0）
2. メモリリークがないこと（Heap Summary: All heap blocks were freed / definitely lost: 0）
3. 終了時に open socket が残っていないこと

## テストケース

- **Idle**: 起動直後（accept待ち）の状態でのシグナル受信
- **Busy**: クライアント接続中（リクエスト送信中）の状態でのシグナル受信

## 実行

```sh
bash sample/08_signal_test/run.sh
```

このスクリプトは `test_signal.py` を実行します。
`ulimit -n` を調整して `valgrind` のアサーションエラーを回避しています。
