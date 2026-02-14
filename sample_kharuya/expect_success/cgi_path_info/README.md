# cgi_path_info

- category: expect_success
- section: CGI（RFC 3875）
- item: `PATH_INFO` が正しい
- port: 30892
- expected_status: 200

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
