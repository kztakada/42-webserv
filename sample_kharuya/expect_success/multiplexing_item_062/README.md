# multiplexing_item_062

- category: expect_success
- section: 多重化・非ブロッキング I/O（課題要件）
- item: 多数同時接続時にクラッシュせず、応答の欠落/停止が起きない（負荷テスト）
- port: 33651
- expected_status: 200

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
