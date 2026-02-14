# multiplexing_cgi_stdin_stdout_i_o_i_o_cgi_read_write

- category: expect_success
- section: 多重化・非ブロッキング I/O（課題要件）
- item: CGI のパイプ（stdin/stdout）I/O も同一の I/O 多重化ループで管理され、CGI の read/write 待ちでサーバー全体がブロックしない
- port: 24439
- expected_status: 200

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
