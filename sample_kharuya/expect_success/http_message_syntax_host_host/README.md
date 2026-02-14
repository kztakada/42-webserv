# http_message_syntax_host_host

- category: expect_success
- section: HTTP/1.1 基本構文・メッセージ
- item: ヘッダーの大文字小文字非依存で扱える（`Host`/`host` 等）
- port: 21078
- expected_status: 200

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
