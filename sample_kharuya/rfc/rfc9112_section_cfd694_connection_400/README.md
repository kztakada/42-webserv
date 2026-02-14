# rfc9112_section_cfd694_connection_400

- category: rfc
- section: RFC9112: メッセージ構文違反
- item: `Connection` ヘッダの値が不正形式（400）
- port: 30681
- expected_status: 400

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close, keep alive\r\n
\r\n

```
