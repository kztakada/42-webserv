# rfc3875_cgi_1_1_query_string_url

- category: rfc
- section: RFC3875: CGI 1.1 メタ変数
- item: `QUERY_STRING` がURLデコードせずに設定される
- port: 25133
- expected_status: 200

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
