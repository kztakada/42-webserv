# rfc9112_section_cfd694_http_http_1_0_400_505

- category: rfc
- section: RFC9112: メッセージ構文違反
- item: 要求行のHTTPバージョンが `HTTP/1.0` / 不明値（400/505）
- port: 23406
- expected_status: 200

## Request

```text
GET / HTTP/1.0\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
