# cgi_content_length

- category: expect_success
- section: CGI（RFC 3875）
- item: `CONTENT_LENGTH` が正しい
- port: 35955
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
