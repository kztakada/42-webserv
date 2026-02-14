# connection_close

- category: expect_success
- section: 接続管理
- item: `Connection: close` を尊重して接続を閉じる
- port: 34877
- expected_status: 200

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
