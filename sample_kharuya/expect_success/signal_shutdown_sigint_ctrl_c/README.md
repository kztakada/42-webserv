# signal_shutdown_sigint_ctrl_c

- category: expect_success
- section: シグナル/クリーン終了（課題要件）
- item: `SIGINT`（Ctrl+C）などで終了させた時に、即座にソケットをクローズして終了できる
- port: 22264
- expected_status: 200

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
