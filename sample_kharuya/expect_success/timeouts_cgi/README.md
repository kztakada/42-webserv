# timeouts_cgi

- category: expect_success
- section: タイムアウト・リソース
- item: CGIのタイムアウトを超えたら強制終了できる
- port: 39310
- expected_status: 200

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
