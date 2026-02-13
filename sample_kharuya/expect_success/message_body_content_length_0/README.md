# message_body_content_length_0

- category: expect_success
- section: メッセージボディ
- item: `Content-Length: 0` を正しく扱える
- port: 34116
- expected_status: 200

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
Content-Length: 0\r\n
\r\n

```
