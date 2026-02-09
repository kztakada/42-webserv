# 09_keep_alive

`keep-alive` (Persistent Connections) の動作確認サンプルです。

## 起動

```sh
./webserv sample/09_keep_alive/webserv.conf
```

- listen: `127.0.0.1:18091`

## テスト内容

Python スクリプト `test_keep_alive.py` を使用して、**1つの TCP コネクション** で
2回連続してリクエスト (`GET /index.html`) を送信し、両方のレスポンスが正常に返ってくるか確認します。

HTTP/1.1 ではデフォルトで `Connection: keep-alive` が有効であることが期待されます。
サーバーが 1 回目のレスポンス後に切断してしまうと、2 回目のリクエスト送信またはレスポンス受信に失敗します。

## 実行

```sh
python3 sample/09_keep_alive/test_keep_alive.py
```

成功すると `Keep-alive test passed` と表示されます。
