# required_header_host_ip_127_0_0_1_1

- category: expect_success
- section: 必須ヘッダー
- item: `Host` がIPリテラル（`127.0.0.1` / `[::1]`）でも正しく受理できる
- port: 37114
- expected_status: 200

## Request

```text
GET / HTTP/1.1\r\n
Host: [::1]\r\n
Connection: close\r\n
\r\n

```
