# body_chunk_item_109

- category: expect_failure
- section: ボディ・チャンク
- item: 最終チャンクの後の終端が不正
- port: 21873
- expected_status: 400

## Request

```text
POST / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
Transfer-Encoding: chunked\r\n
\r\n
1\r\n
x\r\n
0\r\n
x\r\n

```
