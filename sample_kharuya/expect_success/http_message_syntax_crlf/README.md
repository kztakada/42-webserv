# http_message_syntax_crlf

- category: expect_success
- section: HTTP/1.1 基本構文・メッセージ
- item: 先頭行のCRLF終端を受理できる
- port: 28774
- expected_status: 200

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
