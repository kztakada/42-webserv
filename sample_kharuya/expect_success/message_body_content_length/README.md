# message_body_content_length

- category: expect_success
- section: メッセージボディ
- item: `Content-Length` に基づいてボディを読み込める
- port: 29631
- expected_status: 200

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
Content-Length: 5\r\n
\r\n
hello
```
