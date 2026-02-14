# rfc9112_section_c619b2_head

- category: rfc
- section: RFC9112: ステータスコード
- item: HEAD でボディを送らずヘッダのみ返却される
- port: 26262
- expected_status: 501

## Request

```text
HEAD / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
