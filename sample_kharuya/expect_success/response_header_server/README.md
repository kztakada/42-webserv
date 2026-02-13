# response_header_server

- category: expect_success
- section: ヘッダ生成
- item: `Server` ヘッダーを返せる（任意だが一般的）
- port: 24321
- expected_status: 200

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
