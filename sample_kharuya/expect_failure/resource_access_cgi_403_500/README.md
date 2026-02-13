# resource_access_cgi_403_500

- category: expect_failure
- section: 不正なリソースアクセス（堅牢性）
- item: 実行権限のない CGI スクリプトにアクセスした場合に適切なエラーコードを返す（例: `403` または `500` の方針に従う）
- port: 24688
- expected_status: 403

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
