# cgi_502

- category: expect_failure
- section: CGI（RFC 3875）
- item: CGIが異常終了しても `502` にならない
- port: 33146
- expected_status: 502

## Request

```text
GET /cgi/fail.py HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
