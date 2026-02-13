# cgi_server_protocol_http_1_1

- category: expect_success
- section: CGI（RFC 3875）
- item: `SERVER_PROTOCOL` が正しい（`HTTP/1.1`）
- port: 24715
- expected_status: 200

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
