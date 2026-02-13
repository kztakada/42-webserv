# http_message_syntax_get_http_1_1

- category: expect_success
- section: HTTP/1.1 基本構文・メッセージ
- item: リクエストライン: `GET / HTTP/1.1` を受理できる
- port: 39033
- expected_status: 200

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```

