# rfc3875_cgi_server_protocol_http_1_1

- category: rfc
- section: RFC3875: CGI メタ変数不整合
- item: `SERVER_PROTOCOL` が `HTTP/1.1` 以外（失敗）
- port: 39698
- expected_status: 200

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
