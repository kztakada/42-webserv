# http_message_syntax_item_007

- category: expect_success
- section: HTTP/1.1 基本構文・メッセージ
- item: 空行でヘッダー終端を認識できる
- port: 37950
- expected_status: 200

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
