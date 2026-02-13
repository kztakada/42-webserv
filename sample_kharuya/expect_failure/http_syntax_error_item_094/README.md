# http_syntax_error_item_094

- category: expect_failure
- section: HTTP/1.1 構文エラー
- item: ヘッダーに `:` が無い
- port: 23822
- expected_status: 400

## Request

```text
GET / HTTP/1.1\r\n
Host localhost\r\n
Connection: close\r\n
\r\n

```
