# http_syntax_error_item_093

- category: expect_failure
- section: HTTP/1.1 構文エラー
- item: ヘッダーフィールド名に空白や制御文字が含まれる
- port: 28556
- expected_status: 400

## Request

```text
GET / HTTP/1.1\r\n
Bad Header: x\r\n
Connection: close\r\n
\r\n

```
