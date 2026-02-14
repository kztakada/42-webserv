# routing_location_longest_prefix_match_images_images_logo_png_images

- category: expect_success
- section: ルーティング / パス解決
- item: location の優先順位が最長一致（Longest Prefix Match）になる（例: `/` と `/images/` がある時に `/images/logo.png` は `/images/` が選ばれる）
- port: 21633
- expected_status: 200

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
