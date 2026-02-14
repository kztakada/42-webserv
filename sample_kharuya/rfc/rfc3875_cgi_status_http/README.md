# rfc3875_cgi_status_http

- category: rfc
- section: RFC3875: CGI 入出力
- item: CGI が `Status:` ヘッダを返した場合にそれをHTTPステータスへ反映
- port: 27844
- expected_status: 200

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
