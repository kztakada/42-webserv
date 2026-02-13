# status_header_date

- category: expect_failure
- section: ステータス/ヘッダー
- item: `Date` ヘッダーが欠落（方針次第だが一般的には必要）
- port: 31505
- expected_status: 400

## Request

```text
GET / HTTP/1.1\r\n
Connection: close\r\n
\r\n

```
