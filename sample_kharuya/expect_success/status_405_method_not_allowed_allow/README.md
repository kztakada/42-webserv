# status_405_method_not_allowed_allow

- category: expect_success
- section: ステータスコード
- item: `405 Method Not Allowed` を返せる（Allowヘッダー必須）
- port: 36232
- expected_status: 405

## Request

```text
DELETE /only_get_post HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
