# status_505_http_version_not_supported

- category: expect_success
- section: ステータスコード
- item: `505 HTTP Version Not Supported` を返せる
- port: 28771
- expected_status: 505

## Request

```text
GET / HTTP/9.9\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
