# http_syntax_error_crlf_lf

- category: expect_failure
- section: HTTP/1.1 構文エラー
- item: CRLFではなくLFのみのリクエスト（要件により拒否）
- port: 29106
- expected_status: 400

## Request

```text
GET / HTTP/1.1\n
Connection: close\n
\n

```
