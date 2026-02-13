# rfc9112_section_735d1c_400

- category: rfc
- section: RFC9112: リクエストターゲット不正
- item: パス中に不正な制御文字が含まれる（400）
- port: 25837
- expected_status: 400

## Request

```text
GET /a\x01b HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
