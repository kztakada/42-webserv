# rfc9112_section_7a7a60_204_304

- category: rfc
- section: RFC9112: 応答不整合
- item: 204/304 でボディが返ってくる（失敗）
- port: 27624
- expected_status: 204

## Request

```text
DELETE /delete_me.txt HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
