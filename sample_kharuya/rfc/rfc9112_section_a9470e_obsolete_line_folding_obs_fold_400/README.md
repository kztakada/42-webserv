# rfc9112_section_a9470e_obsolete_line_folding_obs_fold_400

- category: rfc
- section: RFC9112: 改行・空白・ヘッダ構文
- item: obsolete line folding（obs-fold）は受理せず 400 を返す
- port: 33579
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
