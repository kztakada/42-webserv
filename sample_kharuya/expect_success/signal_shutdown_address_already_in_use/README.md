# signal_shutdown_address_already_in_use

- category: expect_success
- section: シグナル/クリーン終了（課題要件）
- item: 終了直後に再起動してもポートを再バインドできる（`Address already in use` にならない）
- port: 35944
- expected_status: 200

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
