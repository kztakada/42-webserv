# status_413_payload_too_large

- category: expect_success
- section: ステータスコード
- item: `413 Payload Too Large` を返せる
- port: 33058
- expected_status: 413

## Request

```text
POST / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
Content-Length: 32\r\n
\r\n
aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa
```
