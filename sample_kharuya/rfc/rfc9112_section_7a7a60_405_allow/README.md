# rfc9112_section_7a7a60_405_allow

- category: rfc
- section: RFC9112: 応答不整合
- item: 405 で `Allow` が無い（失敗）
- port: 29332
- expected_status: 405

## Request

```text
DELETE /only_get_post HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
