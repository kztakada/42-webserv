# cgi

- category: expect_success
- section: CGI（RFC 3875）
- item: CGIが標準エラーに出力してもサーバーが落ちない
- port: 21212
- expected_status: 200

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
