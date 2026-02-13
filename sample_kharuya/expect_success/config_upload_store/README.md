# config_upload_store

- category: expect_success
- section: 設定ファイル（Configuration：課題要件）
- item: アップロード保存先（upload store）の設定が反映され、アップロードされたファイルが指定ディレクトリに保存される
- port: 24872
- expected_status: 200

## Request

```text
GET / HTTP/1.1\r\n
Host: localhost\r\n
Connection: close\r\n
\r\n

```
