# cgi_details_cgi_content_length_cgi_eof

- category: expect_success
- section: CGI（課題PDF由来の細部）
- item: CGI が `Content-Length` を返さない場合、CGI プロセス終了（EOF）をレスポンス終端として正しく扱える
- port: 28367
- expected_status: 200

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
Content-Length: 5\r\n
\r\n
hello
```
