# rfc3875_cgi_1_1_script_name_path_info_path_translated

- category: rfc
- section: RFC3875: CGI 1.1 メタ変数
- item: `SCRIPT_NAME` / `PATH_INFO` / `PATH_TRANSLATED` の整合が正しい
- port: 23003
- expected_status: 200

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
