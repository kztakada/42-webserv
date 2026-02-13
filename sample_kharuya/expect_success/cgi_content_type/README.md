# cgi_content_type

- category: expect_success
- section: CGI（RFC 3875）
- item: CGIがボディのみ返した場合のデフォルト `Content-Type` を補う
- port: 25972
- expected_status: 200

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
