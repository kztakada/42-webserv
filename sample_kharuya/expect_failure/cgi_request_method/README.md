# cgi_request_method

- category: expect_failure
- section: CGI（RFC 3875）
- item: CGI環境変数不足（`REQUEST_METHOD` など）
- port: 21448
- expected_status: 400

## Request

```text
GET / HTTP/1.1\r\n
Connection: close\r\n
\r\n

```
