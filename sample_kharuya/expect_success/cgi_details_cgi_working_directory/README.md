# cgi_details_cgi_working_directory

- category: expect_success
- section: CGI（課題PDF由来の細部）
- item: CGI が相対パスでファイルアクセスする場合に備え、適切な作業ディレクトリ（working directory）で実行される
- port: 38298
- expected_status: 200

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
