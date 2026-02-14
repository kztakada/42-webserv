# rfc3875_cgi_content_length

- category: rfc
- section: RFC3875: CGI メタ変数不整合
- item: `CONTENT_LENGTH` と実際の長さが不一致（失敗）
- port: 29089
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
