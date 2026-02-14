# cgi_details_transfer_encoding_chunked_un_chunk_cgi_stdin

- category: expect_success
- section: CGI（課題PDF由来の細部）
- item: `Transfer-Encoding: chunked` のリクエストボディをサーバー側で un-chunk してから CGI の stdin に渡せる
- port: 26725
- expected_status: 200

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
Transfer-Encoding: chunked\r\n
\r\n
4\r\n
Wiki\r\n
0\r\n
\r\n

```
