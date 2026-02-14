# connection_persistence_connection_close

- category: expect_failure
- section: 接続/持続
- item: `Connection: close` でも接続が閉じない
- port: 30113
- expected_status: 400

## Request

```text
GET / HTTP/1.1\r\n
Connection: close\r\n
\r\n

```
