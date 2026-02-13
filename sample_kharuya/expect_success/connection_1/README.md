# connection_1

- category: expect_success
- section: 接続管理
- item: 1接続で複数リクエストを順序通り処理できる（パイプライン）
- port: 22633
- expected_status: 200

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
