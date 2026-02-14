# http_message_syntax_sp

- category: expect_success
- section: HTTP/1.1 基本構文・メッセージ
- item: リクエストライン: メソッド/ターゲット/バージョンが単一SP区切りで解析できる
- port: 20454
- expected_status: 200

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
