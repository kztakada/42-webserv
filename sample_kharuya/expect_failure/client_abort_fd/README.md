# client_abort_fd

- category: expect_failure
- section: クライアント突然切断（課題要件）
- item: レスポンス送信途中にクライアントが切断すると、FD がリークする/セッションが残留する
- port: 27178
- expected_status: 400

## Request

```text
GET / HTTP/1.1\r\n
Connection: close\r\n
\r\n

```
