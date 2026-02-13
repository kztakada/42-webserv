# cgi_remote_addr

- category: expect_success
- section: CGI（RFC 3875）
- item: `REMOTE_ADDR` が正しい
- port: 38845
- expected_status: 200

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
