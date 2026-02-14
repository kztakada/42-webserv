# multiplexing_server_block

- category: expect_success
- section: 多重化・非ブロッキング I/O（課題要件）
- item: ポートごとに異なる server block 設定が適用され、異なるコンテンツを配信できる
- port: 23841
- expected_status: 200

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
