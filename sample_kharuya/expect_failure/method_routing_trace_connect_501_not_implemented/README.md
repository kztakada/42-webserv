# method_routing_trace_connect_501_not_implemented

- category: expect_failure
- section: メソッド/ルーティング
- item: 未実装メソッド（`TRACE`, `CONNECT`）へのアクセス（`501 Not Implemented`）
- port: 32928
- expected_status: 405

## Request

```text
TRACE / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
