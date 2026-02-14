# rfc3875_cgi_http

- category: rfc
- section: RFC3875: CGI メタ変数不整合
- item: `HTTP_*` 変数が欠落している（失敗）
- port: 32483
- expected_status: 200

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
