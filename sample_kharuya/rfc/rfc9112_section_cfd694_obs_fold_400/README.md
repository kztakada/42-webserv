# rfc9112_section_cfd694_obs_fold_400

- category: rfc
- section: RFC9112: メッセージ構文違反
- item: obs-fold を含むヘッダ（400）
- port: 20280
- expected_status: 400

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
X-Fold: a\r\n
\tb\r\n
Connection: close\r\n
\r\n

```
