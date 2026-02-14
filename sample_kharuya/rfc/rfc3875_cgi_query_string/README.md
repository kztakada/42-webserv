# rfc3875_cgi_query_string

- category: rfc
- section: RFC3875: CGI メタ変数不整合
- item: `QUERY_STRING` がデコード済みで渡される（失敗）
- port: 23297
- expected_status: 200

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
