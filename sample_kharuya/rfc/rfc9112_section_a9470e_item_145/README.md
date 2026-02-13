# rfc9112_section_a9470e_item_145

- category: rfc
- section: RFC9112: 改行・空白・ヘッダ構文
- item: フィールド名の不正文字（制御文字、スペース等）を拒否
- port: 35308
- expected_status: 400

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Bad Header: x\r\n
Connection: close\r\n
\r\n

```
