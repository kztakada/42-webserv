# config_url_301_302_location

- category: expect_success
- section: 設定ファイル（Configuration：課題要件）
- item: リダイレクト設定が反映され、指定URLへの `301/302` が正しく返る（`Location` ヘッダ含む）
- port: 26547
- expected_status: 301

## Request

```text
GET /redir HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
