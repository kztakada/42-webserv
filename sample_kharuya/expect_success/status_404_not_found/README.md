# status_404_not_found

- category: expect_success
- section: ステータスコード
- item: `404 Not Found` を返せる
- port: 31958
- expected_status: 404

## Request

```text
GET /no_such_file.html HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
