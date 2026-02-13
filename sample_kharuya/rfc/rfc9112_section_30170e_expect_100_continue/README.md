# rfc9112_section_30170e_expect_100_continue

- category: rfc
- section: RFC9112: メッセージボディ不整合
- item: `Expect: 100-continue` に対して本文を待たずに拒否応答しない（失敗として検出）
- port: 21791
- expected_status: 200

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
