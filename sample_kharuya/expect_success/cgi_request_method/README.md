# cgi_request_method

- category: expect_success
- section: CGI（RFC 3875）
- item: `REQUEST_METHOD` が正しい
- port: 35375
- expected_status: 200

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
