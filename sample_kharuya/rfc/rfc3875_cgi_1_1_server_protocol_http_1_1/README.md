# rfc3875_cgi_1_1_server_protocol_http_1_1

- category: rfc
- section: RFC3875: CGI 1.1 メタ変数
- item: `SERVER_PROTOCOL` が `HTTP/1.1` になる
- port: 21927
- expected_status: 200

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
