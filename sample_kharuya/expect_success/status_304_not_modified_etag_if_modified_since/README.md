# status_304_not_modified_etag_if_modified_since

- category: expect_success
- section: ステータスコード
- item: `304 Not Modified` を条件付きリクエストに応じて返せる（ETag/If-Modified-Since）
- port: 32642
- expected_status: 200

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
