# config_error_page_404_500

- category: expect_success
- section: 設定ファイル（Configuration：課題要件）
- item: `error_page`（カスタムエラーページ）が設定通りに表示される（404/500 など）
- port: 36536
- expected_status: 404

## Request

```text
GET /no_such_file.html HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
