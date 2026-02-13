# routing_item_049

- category: expect_failure
- section: ルーティング / パス解決
- item: パストラバーサル（`../`）を拒否できる
- port: 31388
- expected_status: 400

## Request

```text
GET /../ HTTP/1.1\r\n
Connection: close\r\n
\r\n

```
