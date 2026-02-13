# rfc9112_section_cfd694_transfer_encoding_400_501

- category: rfc
- section: RFC9112: メッセージ構文違反
- item: 不明な `Transfer-Encoding` の指定（400/501）
- port: 29452
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
