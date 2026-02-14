# rfc3875_cgi_1_1_content_type_content_length

- category: rfc
- section: RFC3875: CGI 1.1 メタ変数
- item: `CONTENT_TYPE` / `CONTENT_LENGTH` が正しく設定される
- port: 31188
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
