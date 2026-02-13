# cgi_status

- category: expect_failure
- section: CGI（RFC 3875）
- item: CGIが `Status:` を返しても反映されない
- port: 36001
- expected_status: 400

## Request

```text
GET / HTTP/1.1\r\n
Connection: close\r\n
\r\n

```
