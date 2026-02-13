# http_message_syntax_field_name_field_value_ows

- category: expect_success
- section: HTTP/1.1 基本構文・メッセージ
- item: ヘッダーフィールド: `field-name: field-value` を解析できる（OWSの許容）
- port: 21740
- expected_status: 200

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
