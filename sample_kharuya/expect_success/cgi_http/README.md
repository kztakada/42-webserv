# cgi_http

- category: expect_success
- section: CGI（RFC 3875）
- item: CGI標準出力がHTTPレスポンスに正しく変換される
- port: 27271
- expected_status: 200

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
