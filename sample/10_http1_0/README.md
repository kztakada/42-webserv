# 10_http1_0/

HTTP/1.0 クライアント互換のスモークテスト素材です。

## 目的

- HTTP/1.0 では `Transfer-Encoding: chunked` を使わない
- HTTP/1.0 のデフォルトは connection close
- HTTP/1.0 では `Host` ヘッダー無しでもリクエストできる（HTTP/1.1 の必須要件ではない）
- `Connection: keep-alive` + 静的コンテンツ（Content-Lengthが確定できる）なら同一接続で連続リクエスト可能
- `Connection: close` を明示した場合は、必ずレスポンス送信後に切断される
- `Connection: keep-alive` があっても、ボディ長が事前確定できないレスポンス（CGI想定）の場合は close で終端する

## 実行

リポジトリルートで:

```sh
./webserv sample/10_http1_0/webserv.conf
python3 sample/10_http1_0/test_http1_0.py
```
