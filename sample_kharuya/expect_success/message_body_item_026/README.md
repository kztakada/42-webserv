# message_body_item_026

- category: expect_success
- section: メッセージボディ
- item: トレーラーヘッダーを受理できる（もしくは明示的に拒否）
- port: 21444
- expected_status: 200

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
