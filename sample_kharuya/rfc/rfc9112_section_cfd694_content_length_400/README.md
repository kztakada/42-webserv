# rfc9112_section_cfd694_content_length_400

- category: rfc
- section: RFC9112: メッセージ構文違反
- item: `Content-Length` が複数で不一致（400）
- port: 30092
- expected_status: 400

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
Content-Length: 5\r\n
Content-Length: 6\r\n
\r\n
hello!
```
