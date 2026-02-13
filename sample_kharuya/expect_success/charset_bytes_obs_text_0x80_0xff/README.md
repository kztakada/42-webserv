# charset_bytes_obs_text_0x80_0xff

- category: expect_success
- section: 文字セット・バイト許容
- item: ヘッダーフィールド値に `obs-text`（0x80-0xFF）が含まれても扱える（または明示的に拒否）
- port: 29313
- expected_status: 200

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
