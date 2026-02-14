# rfc9112_section_44aa71_asterisk_form_options

- category: rfc
- section: RFC9112: リクエストターゲット
- item: asterisk-form（`OPTIONS *`）に対する適切な応答
- port: 36976
- expected_status: 405

## Request

```text
OPTIONS * HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
