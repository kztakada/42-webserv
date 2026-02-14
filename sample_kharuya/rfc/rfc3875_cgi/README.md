# rfc3875_cgi

- category: rfc
- section: RFC3875: CGI 出力不整合
- item: CGI がヘッダ終端の空行を出力しないのにボディを解釈する（失敗）
- port: 24829
- expected_status: 200

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
