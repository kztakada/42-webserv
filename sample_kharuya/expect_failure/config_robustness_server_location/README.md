# config_robustness_server_location

- category: expect_failure
- section: 設定ファイル/起動の堅牢性（課題要件）
- item: server/location のネストや必須要素不足など、設定の構造不正で安全に失敗する
- port: 38311
- expected_status: 400

## Request

```text
GET / HTTP/1.1\r\n
Connection: close\r\n
\r\n

```
