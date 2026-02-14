# config_location_get_post_405_method_not_allowed

- category: expect_success
- section: 設定ファイル（Configuration：課題要件）
- item: ルート（location）ごとの許可メソッド設定が反映される（例: GETのみ許可しPOSTで `405 Method Not Allowed`）
- port: 20607
- expected_status: 405

## Request

```text
DELETE /only_get_post HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
