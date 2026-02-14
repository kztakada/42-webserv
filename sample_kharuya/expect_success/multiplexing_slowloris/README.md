# multiplexing_slowloris

- category: expect_success
- section: 多重化・非ブロッキング I/O（課題要件）
- item: スロークライアント（Slowloris）相当の非常に遅い送信があっても、他接続の処理がブロックされず進む
- port: 20127
- expected_status: 200

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
