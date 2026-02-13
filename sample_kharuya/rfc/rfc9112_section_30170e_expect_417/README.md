# rfc9112_section_30170e_expect_417

- category: rfc
- section: RFC9112: メッセージボディ不整合
- item: `Expect` に未知のトークンがある場合に 417 を返さない（失敗として検出）
- port: 36714
- expected_status: 200

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
