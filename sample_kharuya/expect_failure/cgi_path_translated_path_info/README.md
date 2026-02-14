# cgi_path_translated_path_info

- category: expect_failure
- section: CGI（RFC 3875）
- item: `PATH_TRANSLATED` が `PATH_INFO` と整合しない（誤った物理パスになる/未設定になる）
- port: 39894
- expected_status: 400

## Request

```text
GET / HTTP/1.1\r\n
Connection: close\r\n
\r\n

```
