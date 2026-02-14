# rfc9112_section_961c6d_transfer_encoding_content_length_transfer_encoding

- category: rfc
- section: RFC9112: メッセージボディと長さ判定
- item: `Transfer-Encoding` と `Content-Length` が同時にある場合は `Transfer-Encoding` を優先
- port: 27093
- expected_status: 501

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
Transfer-Encoding: gzip, chunked\r\n
Content-Length: 5\r\n
\r\n
5\r\n
hello\r\n
0\r\n
\r\n
```
