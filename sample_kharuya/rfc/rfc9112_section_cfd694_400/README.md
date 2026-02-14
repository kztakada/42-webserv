# rfc9112_section_cfd694_400

- category: rfc
- section: RFC9112: メッセージ構文違反
- item: フィールド名に空白や制御文字を含む（400）
- port: 29344
- expected_status: 400

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Bad Header: x\r\n
Connection: close\r\n
\r\n

```
