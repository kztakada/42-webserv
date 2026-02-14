# http_syntax_error_transfer_encoding_chunked_te

- category: expect_failure
- section: HTTP/1.1 構文エラー
- item: `Transfer-Encoding` が `chunked` 以外のみ（未対応TEなら拒否）
- port: 24626
- expected_status: 400

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
Transfer-Encoding: gzip\r\n
\r\n

```
