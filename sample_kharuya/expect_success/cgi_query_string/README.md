# cgi_query_string

- category: expect_success
- section: CGI（RFC 3875）
- item: `QUERY_STRING` が正しい
- port: 32424
- expected_status: 200

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
