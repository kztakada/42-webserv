# rfc9112_section_cfd694_transfer_encoding_chunked_400

- category: rfc
- section: RFC9112: メッセージ構文違反
- item: `Transfer-Encoding` の最後が `chunked` でない（400）
- port: 21451
- expected_status: 400

## Request

```text
POST / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
Transfer-Encoding: chunked, gzip\r\n
\r\n

```
