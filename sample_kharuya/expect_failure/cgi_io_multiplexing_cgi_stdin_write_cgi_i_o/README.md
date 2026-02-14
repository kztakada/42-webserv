# cgi_io_multiplexing_cgi_stdin_write_cgi_i_o

- category: expect_failure
- section: CGI I/O 多重化（課題要件）
- item: CGI へ巨大入力を送信中（stdin write）に、別クライアントの静的ファイル応答が極端に遅延する（CGI I/O がブロックしている）
- port: 35290
- expected_status: 400

## Request

```text
GET / HTTP/1.1\r\n
Connection: close\r\n
\r\n

```
