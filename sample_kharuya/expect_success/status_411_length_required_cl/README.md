# status_411_length_required_cl

- category: expect_success
- section: ステータスコード
- item: `411 Length Required` を返せる（ボディ必須時にCL未指定）
- port: 21669
- expected_status: 411

## Request

```text
POST / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n
hello
```
