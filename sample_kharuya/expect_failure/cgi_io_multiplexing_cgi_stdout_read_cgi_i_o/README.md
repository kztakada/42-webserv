# cgi_io_multiplexing_cgi_stdout_read_cgi_i_o

- category: expect_failure
- section: CGI I/O 多重化（課題要件）
- item: CGI が巨大出力を生成中（stdout read）に、別クライアント処理が停止する（CGI I/O がブロックしている）
- port: 21419
- expected_status: 400

## Request

```text
GET / HTTP/1.1\r\n
Connection: close\r\n
\r\n

```
