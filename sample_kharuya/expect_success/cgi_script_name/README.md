# cgi_script_name

- category: expect_success
- section: CGI（RFC 3875）
- item: `SCRIPT_NAME` が正しい
- port: 38789
- expected_status: 200

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
