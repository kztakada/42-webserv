# charset_bytes_request_target_options_http_1_1

- category: expect_failure
- section: 文字セット・バイト許容
- item: Request Target に `*`（`OPTIONS * HTTP/1.1`）を受理できる
- port: 31806
- expected_status: 405

## Request

```text
OPTIONS * HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
