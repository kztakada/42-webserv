# rfc9112_section_cfd694_field_name_value_400

- category: rfc
- section: RFC9112: メッセージ構文違反
- item: ヘッダ行が `field-name: value` 形式でない（400）
- port: 28330
- expected_status: 400

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
BadHeader x\r\n
Connection: close\r\n
\r\n

```
