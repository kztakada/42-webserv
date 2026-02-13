# status_header_405_allow

- category: expect_failure
- section: ステータス/ヘッダー
- item: `405` で `Allow` が無い
- port: 23369
- expected_status: 405

## Request

```text
DELETE /delete.txt HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
