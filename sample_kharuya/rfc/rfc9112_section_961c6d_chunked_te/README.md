# rfc9112_section_961c6d_chunked_te

- category: rfc
- section: RFC9112: メッセージボディと長さ判定
- item: `chunked` が最後にある場合のみ受理（他のTEが先行する場合は拒否）
- port: 39382
- expected_status: 200

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
Transfer-Encoding: chunked\r\n
\r\n
4\r\n
Wiki\r\n
0\r\n
\r\n

```
