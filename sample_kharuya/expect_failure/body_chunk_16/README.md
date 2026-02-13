# body_chunk_16

- category: expect_failure
- section: ボディ・チャンク
- item: チャンクサイズが16進数でない
- port: 37103
- expected_status: 400

## Request

```text
POST / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
Transfer-Encoding: chunked\r\n
\r\n
Z\r\n
x\r\n
0\r\n
\r\n

```
