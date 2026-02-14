# rfc9112_section_30170e_content_length_400

- category: rfc
- section: RFC9112: メッセージボディ不整合
- item: `Content-Length` 付きでボディが短い/長い（400）
- port: 33102
- expected_status: 400

## Request

```text
POST / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
Content-Length: 5\r\n
Content-Length: 4\r\n
\r\n
hell
```
