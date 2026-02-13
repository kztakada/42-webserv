# method_routing_405_method_not_allowed

- category: expect_failure
- section: メソッド/ルーティング
- item: 許可されていないメソッドへのアクセス（`405 Method Not Allowed`）
- port: 28843
- expected_status: 405

## Request

```text
DELETE /only_get_post HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
