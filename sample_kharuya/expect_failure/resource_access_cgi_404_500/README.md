# resource_access_cgi_404_500

- category: expect_failure
- section: 不正なリソースアクセス（堅牢性）
- item: 存在しない CGI スクリプトにアクセスした場合に適切なエラーコードを返す（例: `404` または `500` の方針に従う）
- port: 27720
- expected_status: 404

## Request

```text
GET /cgi/no_such.py HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
