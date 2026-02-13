# rfc3875_cgi_stdin

- category: rfc
- section: RFC3875: CGI 入出力
- item: リクエストボディが stdin に正しく渡る
- port: 37073
- expected_status: 200

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
