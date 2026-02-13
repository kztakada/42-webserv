# response_header_content_length_chunked

- category: expect_success
- section: ヘッダ生成
- item: `Content-Length` を正しく返せる（chunkedでない場合）
- port: 24935
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
