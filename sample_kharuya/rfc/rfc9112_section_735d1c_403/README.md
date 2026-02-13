# rfc9112_section_735d1c_403

- category: rfc
- section: RFC9112: リクエストターゲット不正
- item: 正規化後にルート外へ出るパス（403）
- port: 26922
- expected_status: 200

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
