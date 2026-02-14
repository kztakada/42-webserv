# rfc9112_section_44aa71_authority_form_connect_405_501

- category: rfc
- section: RFC9112: リクエストターゲット
- item: authority-form（CONNECT）は拒否（未対応なら 405/501）
- port: 32498
- expected_status: 405S

## Request

```text
CONNECT example.com:443 HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
