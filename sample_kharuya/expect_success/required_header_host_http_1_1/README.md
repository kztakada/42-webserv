# required_header_host_http_1_1

- category: expect_success
- section: 必須ヘッダー
- item: `Host` ヘッダー必須（HTTP/1.1）を満たす
- port: 34961
- expected_status: 200

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
