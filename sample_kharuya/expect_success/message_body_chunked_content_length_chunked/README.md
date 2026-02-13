# message_body_chunked_content_length_chunked

- category: expect_success
- section: メッセージボディ
- item: `chunked` と `Content-Length` が同時にある場合の優先順位（chunked優先）を満たす
- port: 23895
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
