# required_header_host_example_com_8080

- category: expect_success
- section: 必須ヘッダー
- item: `Host` がポート付き（`example.com:8080`）でも正しく受理できる
- port: 39520
- expected_status: 200

## Request

```text
GET / HTTP/1.1\r\n
Host: example.com:8080\r\n
Connection: close\r\n
\r\n

```
