# client_abort_sigpipe

- category: expect_failure
- section: クライアント突然切断（課題要件）
- item: リクエストボディ送信途中にクライアントが切断すると、サーバーが `SIGPIPE` 等でクラッシュする
- port: 29503
- expected_status: 400

## Request

```text
GET / HTTP/1.1\r\n
Connection: close\r\n
\r\n

```
