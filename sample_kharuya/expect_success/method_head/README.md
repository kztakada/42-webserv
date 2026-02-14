# method_head

- category: expect_success
- section: メソッド
- item: `HEAD` を正常に処理できる（ボディなし）
- port: 35265
- expected_status: 405

## Request

```text
HEAD / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
