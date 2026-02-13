# body_chunk_crlf

- category: expect_failure
- section: ボディ・チャンク
- item: チャンクサイズ行の後にCRLFがない
- port: 25205
- expected_status: 400

## Request

```text
POST / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
Transfer-Encoding: chunked\r\n
\r\n
1x\r\n
0\r\n
\r\n

```
