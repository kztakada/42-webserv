# rfc9112_section_cfd694_transfer_encoding_content_length_400

- category: rfc
- section: RFC9112: メッセージ構文違反
- item: `Transfer-Encoding` と `Content-Length` の矛盾（400）
- port: 20839
- expected_status: 501

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
Transfer-Encoding: gzip\r\n
Content-Length: 5\r\n
\r\n
hello
```
