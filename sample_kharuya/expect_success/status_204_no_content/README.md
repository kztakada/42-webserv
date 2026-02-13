# status_204_no_content

- category: expect_success
- section: ステータスコード
- item: `204 No Content` を適切に返せる（ボディなし）
- port: 30195
- expected_status: 204

## Request

```text
DELETE /delete_me.txt HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
