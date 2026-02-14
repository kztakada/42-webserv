# method_routing_images_location_location

- category: expect_failure
- section: メソッド/ルーティング
- item: `/` と `/images/` のようなネストされた location がある場合に、最長一致が守られず誤った location が選ばれる（失敗として検出）
- port: 37650
- expected_status: 400

## Request

```text
GET / HTTP/1.1\r\n
Connection: close\r\n
\r\n

```
