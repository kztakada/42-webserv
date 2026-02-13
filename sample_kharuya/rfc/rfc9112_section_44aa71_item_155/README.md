# rfc9112_section_44aa71_item_155

- category: rfc
- section: RFC9112: リクエストターゲット
- item: パス正規化の前後で `..` / `.` を正しく解釈できる
- port: 23715
- expected_status: 200

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
