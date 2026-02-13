# connection_keep_alive

- category: expect_success
- section: 接続管理
- item: `Connection: keep-alive` で持続接続を維持できる
- port: 28306
- expected_status: 200

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Connection: keep-alive\r\n
\r\n

```
