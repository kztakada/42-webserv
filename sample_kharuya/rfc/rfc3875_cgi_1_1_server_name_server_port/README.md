# rfc3875_cgi_1_1_server_name_server_port

- category: rfc
- section: RFC3875: CGI 1.1 メタ変数
- item: `SERVER_NAME` / `SERVER_PORT` が正しく設定される
- port: 28799
- expected_status: 200

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
