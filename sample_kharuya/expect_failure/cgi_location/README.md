# cgi_location

- category: expect_failure
- section: CGI（RFC 3875）
- item: CGIが `Location:` のみ返した時の処理が誤り（ローカル/外部）
- port: 31299
- expected_status: 400

## Request

```text
GET / HTTP/1.1\r\n
Connection: close\r\n
\r\n

```
