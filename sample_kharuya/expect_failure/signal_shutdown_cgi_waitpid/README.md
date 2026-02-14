# signal_shutdown_cgi_waitpid

- category: expect_failure
- section: シグナル/クリーン終了
- item: 停止時に CGI 子プロセスが残留してゾンビ化する（`waitpid` 不備の疑い）
- port: 24419
- expected_status: 400

## Request

```text
GET / HTTP/1.1\r\n
Connection: close\r\n
\r\n

```
