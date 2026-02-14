# routing_index

- category: expect_success
- section: ルーティング / パス解決
- item: 末尾 `/` 付きのディレクトリに対して `index` が有効
- port: 30946
- expected_status: 200

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
