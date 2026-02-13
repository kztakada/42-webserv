# cgi_504

- category: expect_failure
- section: CGI（RFC 3875）
- item: CGIがタイムアウトした場合に `504` を返す
- port: 33147
- expected_status: 504

## Request

```text
GET /cgi/loop.py HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
