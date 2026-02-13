# cgi_path_translated_path_info_root_path_info

- category: expect_success
- section: CGI（RFC 3875）
- item: `PATH_TRANSLATED` が `PATH_INFO` と root の対応関係に基づいて正しく設定される（`PATH_INFO` がある場合）
- port: 27573
- expected_status: 200

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
