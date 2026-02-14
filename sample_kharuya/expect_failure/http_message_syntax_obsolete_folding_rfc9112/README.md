# http_message_syntax_obsolete_folding_rfc9112

- category: expect_failure
- section: HTTP/1.1 基本構文・メッセージ
- item: ヘッダーフィールドの複数行連結（obsolete folding）は**拒否**または明示的に非対応（RFC9112準拠の処理方針を明確化）
- port: 26501
- expected_status: 400

## Request

```text
GET / HTTP/1.1\r\n
X-Fold: a\r\n
\tb\r\n
Connection: close\r\n
\r\n

```
