# rfc3875_cgi_1_1_server_software

- category: rfc
- section: RFC3875: CGI 1.1 メタ変数
- item: `SERVER_SOFTWARE` が実装の識別子として設定される
- port: 33321
- expected_status: 200

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
