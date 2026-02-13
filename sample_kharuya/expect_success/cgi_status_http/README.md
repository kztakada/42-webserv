# cgi_status_http

- category: expect_success
- section: CGI（RFC 3875）
- item: CGIが `Status:` ヘッダーを返した場合にHTTPステータスに反映される
- port: 22678
- expected_status: 200

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
