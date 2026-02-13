# rfc3875_cgi_stdout_content_type

- category: rfc
- section: RFC3875: CGI 出力不整合
- item: CGI の stdout に `Content-Type` が無い場合の扱いが不正（失敗）
- port: 24828
- expected_status: 200

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
