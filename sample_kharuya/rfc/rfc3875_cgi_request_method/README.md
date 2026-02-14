# rfc3875_cgi_request_method

- category: rfc
- section: RFC3875: CGI メタ変数不整合
- item: `REQUEST_METHOD` が空/不一致（失敗）
- port: 25759
- expected_status: 200

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
