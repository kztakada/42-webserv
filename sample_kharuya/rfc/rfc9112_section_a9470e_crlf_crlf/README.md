# rfc9112_section_a9470e_crlf_crlf

- category: rfc
- section: RFC9112: 改行・空白・ヘッダ構文
- item: 空行（CRLF CRLF）でヘッダ終端を認識する
- port: 30703
- expected_status: 200

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
