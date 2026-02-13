# multiplexing_8080_8081

- category: expect_success
- section: 多重化・非ブロッキング I/O（課題要件）
- item: 複数ポート（例: 8080 と 8081）で同時に待受できる
- port: 39586
- expected_status: 200

## Request

```text
GET / HTTP/1.1\r\n
Host: example.com:8080\r\n
Connection: close\r\n
\r\n

```
