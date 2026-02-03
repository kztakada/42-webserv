# 02_error_page

`error_page` で 400 / 403 / 404 / 405 / 413 / 500 にカスタムHTMLを適用するサンプルです。

## 起動

```sh
./webserv sample/02_error_page/webserv.conf
```

## 確認（405）

```sh
curl -v -X POST http://127.0.0.1:18082/hello.txt
```

## 確認（404）

```sh
curl -v http://127.0.0.1:18082/missing.txt
```

## 確認（403）

```sh
curl -v http://127.0.0.1:18082/errors/
```

## 確認（400）

```sh
curl -v --path-as-is http://127.0.0.1:18082/../secret
```

## 確認（413）

この設定では `/upload` の `client_max_body_size` を 4 bytes にしてあります。

```sh
curl -v http://127.0.0.1:18082/upload \
  -H 'Content-Type: text/plain' \
  --data '0123456789'
```

## 確認（500）

500 は主にサーバー内部エラー（例: CGI ヘッダ不正、内部処理失敗）で発生します。

このサンプルには、**確実に 500 を発生させる CGI**（ローカルリダイレクト無限ループ）を入れてあります。

```sh
curl -i http://127.0.0.1:18082/cgi/loop.bad
```

CGI ヘッダ不正で 500 を踏む場合はこちらです。

```sh
curl -i http://127.0.0.1:18082/cgi/bad_header.bad
```

- `HTTP/1.1 500` が返り、本文が `error_page 500` の `errors/500.html` に差し替わっていればOKです。
