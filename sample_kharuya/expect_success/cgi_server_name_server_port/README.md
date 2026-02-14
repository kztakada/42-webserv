# cgi_server_name_server_port

- category: expect_success
- section: CGI（RFC 3875）
- item: `SERVER_NAME` / `SERVER_PORT` が正しい
- port: 21203
- expected_status: 200

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
