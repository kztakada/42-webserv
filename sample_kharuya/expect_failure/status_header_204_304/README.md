# status_header_204_304

- category: expect_failure
- section: ステータス/ヘッダー
- item: `204` / `304` でボディが返ってしまう
- port: 33007
- expected_status: 204

## Request

```text
DELETE /delete_me.txt HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
