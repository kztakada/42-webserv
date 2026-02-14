# status_header_head

- category: expect_failure
- section: ステータス/ヘッダー
- item: `HEAD` でボディが返ってしまう
- port: 27593
- expected_status: 501

## Request

```text
HEAD / HTTP/1.1\r\n
Connection: close\r\n
\r\n

```
