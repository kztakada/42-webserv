# rfc9112_section_961c6d_content_length_400

- category: rfc
- section: RFC9112: メッセージボディと長さ判定
- item: `Content-Length` が複数で同値なら受理、異なるなら拒否（400）
- port: 29315
- expected_status: 400

## Request

```text
POST / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
Content-Length: 5\r\n
Content-Length: 6\r\n
\r\n
hello!
```
