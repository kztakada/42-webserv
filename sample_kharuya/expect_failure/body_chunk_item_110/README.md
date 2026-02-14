# body_chunk_item_110

- category: expect_failure
- section: ボディ・チャンク
- item: チャンク拡張の構文が不正
- port: 20339
- expected_status: 400

## Request

```text
POST / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
Transfer-Encoding: chunked\r\n
\r\n
1;=bad\r\n
x\r\n
0\r\n
\r\n

```
