# rfc9112_section_a9470e_crlf

- category: rfc
- section: RFC9112: 改行・空白・ヘッダ構文
- item: CRLF が行終端として正しく扱われる
- port: 30005
- expected_status: 200

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
