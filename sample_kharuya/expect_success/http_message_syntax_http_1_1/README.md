# http_message_syntax_http_1_1

- category: expect_success
- section: HTTP/1.1 基本構文・メッセージ
- item: `HTTP/1.1` バージョンを正しく認識できる
- port: 25959
- expected_status: 200

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
