# http_syntax_error_http_1_0_http_2_0

- category: expect_failure
- section: HTTP/1.1 構文エラー
- item: バージョンが `HTTP/1.0` または未知（`HTTP/2.0`）
- port: 31959
- expected_status: 400

## Request

```text
GET / HTTP/1.0\r\n
Connection: close\r\n
\r\n

```
