# connection_persistence_1

- category: expect_failure
- section: 接続/持続
- item: 1つのリクエスト失敗で後続処理が破綻する
- port: 32984
- expected_status: 400

## Request

```text
GET / HTTP/1.1\r\n
Connection: close\r\n
\r\n

```
