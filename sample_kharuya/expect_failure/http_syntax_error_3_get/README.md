# http_syntax_error_3_get

- category: expect_failure
- section: HTTP/1.1 構文エラー
- item: リクエストラインが3要素でない（`GET /` など）
- port: 35008
- expected_status: 400

## Request

```text
GET /\r\n
Connection: close\r\n
\r\n

```
