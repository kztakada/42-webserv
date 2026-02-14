# config_client_max_body_size_413_payload_too_large

- category: expect_success
- section: 設定ファイル（Configuration：課題要件）
- item: 設定ファイルで指定した `client_max_body_size` が反映され、超過時に `413 Payload Too Large` が返る
- port: 36387
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
