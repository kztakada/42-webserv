# cgi

- category: expect_failure
- section: CGI（RFC 3875）
- item: CGIプロセスが終了しないのにレスポンスが返る
- port: 31214
- expected_status: 400

## Request

```text
GET / HTTP/1.1\r\n
Connection: close\r\n
\r\n

```
