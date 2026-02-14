# message_body_transfer_encoding_chunked

- category: expect_success
- section: メッセージボディ
- item: `Transfer-Encoding: chunked` を正しく処理できる
- port: 30196
- expected_status: 200

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
Transfer-Encoding: chunked\r\n
\r\n
4\r\n
Wiki\r\n
0\r\n
\r\n

```
