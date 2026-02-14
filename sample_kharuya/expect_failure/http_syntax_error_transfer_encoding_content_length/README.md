# http_syntax_error_transfer_encoding_content_length

- category: expect_failure
- section: HTTP/1.1 構文エラー
- item: `Transfer-Encoding` と `Content-Length` の両方が矛盾
- port: 30351
- expected_status: 400

## Request

```text
GET / HTTP/1.1\r\n
Connection: close\r\n
Transfer-Encoding: gzip\r\n
Content-Length: 5\r\n
\r\n
hello
```
