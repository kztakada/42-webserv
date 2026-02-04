# 06_cookie

CGI で Cookie を扱う（`Cookie` → `HTTP_COOKIE` 環境変数、`Set-Cookie` の転送）ためのサンプルです。

このサンプルでは、初回アクセスで `WEBSERV_ID` を `Set-Cookie`（`Max-Age` 付き）で発行し、
2回目のアクセスで同じ `WEBSERV_ID` が使われていることを確認します。

## 起動

```sh
./webserv sample/06_cookie/webserv.conf
```

- listen: `127.0.0.1:18088`
- CGI:
  - `GET /cgi/cookie.py`
  - `GET /cgi/cookie.php`

## php-cgi 版（setcookie()/session_start() を素直に使う）

環境に `php-cgi` がある場合は、こちらの conf で起動するとより一般的な PHP CGI の挙動になります。

```sh
./webserv sample/06_cookie/webserv_php_cgi.conf
```

- listen: `127.0.0.1:18089`
- CGI:
  - `GET /cgi/cookie_setcookie.php`

手動確認例:

```sh
curl -i http://127.0.0.1:18089/cgi/cookie_setcookie.php
```

## 手動確認（curl）

### 1回目: Cookie が発行される

```sh
curl -i http://127.0.0.1:18088/cgi/cookie.py
```

期待:
- `HTTP/1.1 200`
- `Set-Cookie: WEBSERV_ID=...; Max-Age=60` が含まれる
- body に `new=1` と `id=...` が含まれる

### 2回目: 発行した Cookie が使われる

1回目の `Set-Cookie` の値（`WEBSERV_ID=...`）をそのまま `Cookie:` で返します。

```sh
curl -i http://127.0.0.1:18088/cgi/cookie.py \
  -H 'Cookie: WEBSERV_ID=<paste>'
```

期待:
- body に `new=0` と、同じ `id=` が含まれる

PHP も同様です。

## 自動テスト（python）

```sh
python3 sample/06_cookie/test_cookie.py
```

- 1回目で `Set-Cookie` と `Max-Age` を検証
- 2回目で同じ Cookie ID が使われることを検証

## 備考（PHP について）

このリポジトリの他サンプル同様、executor は `cgi_extension .php /usr/bin/php;`（cli）を使っています。
cli SAPI は `header()/setcookie()` のヘッダを自動で stdout に出さないため、`cookie.php` では CGI 形式のヘッダを `echo` で出力しています。
`php-cgi` を使う場合は `cookie_setcookie.php` を参照してください。
