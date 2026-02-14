# rfc3875_cgi_location

- category: rfc
- section: RFC3875: CGI 入出力
- item: `Location:` ヘッダを返した場合に適切なリダイレクト処理
- port: 30660
- expected_status: 200

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
