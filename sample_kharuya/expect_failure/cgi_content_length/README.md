# cgi_content_length

- category: expect_failure
- section: CGI（RFC 3875）
- item: `CONTENT_LENGTH` がボディ長と不一致
- port: 34344
- expected_status: 400

## Request

```text
GET / HTTP/1.1\r\n
Connection: close\r\n
Content-Length: 5\r\n
\r\n
hello
```
