# rfc9112_section_7a7a60_head

- category: rfc
- section: RFC9112: 応答不整合
- item: HEAD でボディが返ってくる（失敗）
- port: 25107
- expected_status: 501

## Request

```text
HEAD / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
