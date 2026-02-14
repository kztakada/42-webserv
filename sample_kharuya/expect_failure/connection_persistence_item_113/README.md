# connection_persistence_item_113

- category: expect_failure
- section: 接続/持続
- item: パイプラインでレスポンス順が入れ替わる
- port: 32878
- expected_status: 400

## Request

```text
GET / HTTP/1.1\r\n
Connection: close\r\n
\r\n

```
