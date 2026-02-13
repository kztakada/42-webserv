# body_chunk_content_length

- category: expect_failure
- section: ボディ・チャンク
- item: `Content-Length` より短い/長いボディ送信
- port: 32332
- expected_status: 400

## Request

```text
POST / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
Content-Length: 5\r\n
\r\n
hell
```
