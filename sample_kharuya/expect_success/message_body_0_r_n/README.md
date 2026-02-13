# message_body_0_r_n

- category: expect_success
- section: メッセージボディ
- item: 最終チャンク `0\r\n` で終端できる
- port: 34484
- expected_status: 200

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
