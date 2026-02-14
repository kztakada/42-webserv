# rfc3875_cgi_200

- category: rfc
- section: RFC3875: CGI 出力不整合
- item: CGI が不正なヘッダ行を出力したのに200で返す（失敗）
- port: 27740
- expected_status: 200

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
