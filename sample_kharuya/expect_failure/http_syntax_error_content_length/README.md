# http_syntax_error_content_length

- category: expect_failure
- section: HTTP/1.1 構文エラー
- item: `Content-Length` が複数かつ不一致
- port: 26144
- expected_status: 400

## Request

```text
GET / HTTP/1.1\r\n
Connection: close\r\n
Content-Length: 5\r\n
Content-Length: 6\r\n
\r\n
hello!
```
