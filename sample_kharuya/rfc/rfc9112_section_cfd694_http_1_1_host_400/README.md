# rfc9112_section_cfd694_http_1_1_host_400

- category: rfc
- section: RFC9112: メッセージ構文違反
- item: HTTP/1.1 で `Host` ヘッダが無い要求（400）
- port: 22226
- expected_status: 400

## Request

```text
GET / HTTP/1.1\r\n
Connection: close\r\n
\r\n

```
