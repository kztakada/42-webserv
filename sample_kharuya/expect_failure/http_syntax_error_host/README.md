# http_syntax_error_host

- category: expect_failure
- section: HTTP/1.1 構文エラー
- item: `Host` に無効な形式（空、複数、制御文字）
- port: 20365
- expected_status: 200

## Request

```text
GET / HTTP/1.1\r\n
Host: \r\n
Connection: close\r\n
\r\n

```
