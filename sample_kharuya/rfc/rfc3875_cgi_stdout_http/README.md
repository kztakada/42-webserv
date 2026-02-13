# rfc3875_cgi_stdout_http

- category: rfc
- section: RFC3875: CGI 入出力
- item: CGI の stdout からヘッダを受け取りHTTPレスポンスへ変換できる
- port: 21805
- expected_status: 200

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
