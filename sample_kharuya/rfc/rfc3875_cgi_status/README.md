# rfc3875_cgi_status

- category: rfc
- section: RFC3875: CGI 出力不整合
- item: CGI が `Status:` を返しているのに無視される（失敗）
- port: 24253
- expected_status: 200

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
