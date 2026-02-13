# config_robustness_item_132

- category: expect_failure
- section: 設定ファイル/起動の堅牢性（課題要件）
- item: 設定ファイルに構文エラーがある場合、サーバーがクラッシュせずにエラーを出力して終了する
- port: 20990
- expected_status: 400

## Request

```text
GET / HTTP/1.1\r\n
Connection: close\r\n
\r\n

```
