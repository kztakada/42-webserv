# signal_shutdown_sigint_address_already_in_use

- category: expect_failure
- section: シグナル/クリーン終了
- item: `SIGINT` で停止後、再起動時に `Address already in use` が出て起動できない
- port: 30551
- expected_status: 400

## Request

```text
GET / HTTP/1.1\r\n
Connection: close\r\n
\r\n

```
