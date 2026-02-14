# config_error_page

- category: expect_success
- section: 設定ファイル（Configuration：課題要件）
- item: `error_page` が未設定の場合に、サーバーが用意したデフォルトエラーページが返る
- port: 32075
- expected_status: 200

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
