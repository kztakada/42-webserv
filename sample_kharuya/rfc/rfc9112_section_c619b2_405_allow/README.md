# rfc9112_section_c619b2_405_allow

- category: rfc
- section: RFC9112: ステータスコード
- item: 405 の場合に `Allow` ヘッダが正しく付く
- port: 24477
- expected_status: 405

## Request

```text
DELETE /only_get_post HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
