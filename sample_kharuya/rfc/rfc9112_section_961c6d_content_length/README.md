# rfc9112_section_961c6d_content_length

- category: rfc
- section: RFC9112: メッセージボディと長さ判定
- item: `Content-Length` が単一で正しい場合に正常処理
- port: 27698
- expected_status: 200

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
Content-Length: 5\r\n
\r\n
hello
```
