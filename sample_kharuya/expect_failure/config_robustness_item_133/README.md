# config_robustness_item_133

- category: expect_failure
- section: 設定ファイル/起動の堅牢性（課題要件）
- item: 設定ファイルで未知ディレクティブ/不正値（負数、非数、無効ポート等）がある場合に安全に失敗する
- port: 22225
- expected_status: 400

## Request

```text
GET / HTTP/1.1\r\n
Connection: close\r\n
\r\n

```
