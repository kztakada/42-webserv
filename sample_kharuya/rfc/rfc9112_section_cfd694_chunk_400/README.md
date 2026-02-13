# rfc9112_section_cfd694_chunk_400

- category: rfc
- section: RFC9112: メッセージ構文違反
- item: chunk 本文が宣言サイズと不一致（400）
- port: 27749
- expected_status: 400

## Request

```text
POST / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
Transfer-Encoding: chunked\r\n
\r\n
2\r\n
x\r\n
0\r\n
\r\n

```
