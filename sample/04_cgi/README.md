# 04_cgi

CGI を `cgi_extension`（拡張子→実行バイナリ）で動かすサンプルです。

- `/cgi`: `.py` / `.php` / `.js` / `.sh` を CGI として実行
- `/cgi_py` 等: location ごとに CGI 対象拡張子を分離（マッチした拡張子だけ CGI 実行）

## 前提

以下の実行環境が必要です。

- `/usr/bin/python3`
- `/usr/bin/php`
- `/usr/bin/node`
- `/usr/bin/bash`

## 起動

```sh
./webserv sample/04_cgi/webserv.conf
```

- listen: `127.0.0.1:18084`
- CGI ルート: `sample/04_cgi/www/cgi`

## 確認（GET）

```sh
curl -i "http://127.0.0.1:18084/cgi/hello.py?lang=py"
curl -i "http://127.0.0.1:18084/cgi/hello.php?lang=php"
curl -i "http://127.0.0.1:18084/cgi/hello.js?lang=node"
curl -i "http://127.0.0.1:18084/cgi/hello.sh?lang=bash"
```

期待:
- `HTTP/1.1 200`
- `Content-Type: text/plain`
- body に `python ok` / `php ok` / `node ok` / `bash ok` が含まれる
- `query=lang=...` が含まれる

## 確認（POST body が CGI に渡る）

```sh
curl -i -X POST "http://127.0.0.1:18084/cgi/hello.py" --data "abc"
curl -i -X POST "http://127.0.0.1:18084/cgi/hello.php" --data "def"
curl -i -X POST "http://127.0.0.1:18084/cgi/hello.js" --data "ghi"
curl -i -X POST "http://127.0.0.1:18084/cgi/hello.sh" --data "jkl"
```

期待:
- `HTTP/1.1 200`
- body に `body=abc` など、送信した body が含まれる

## 確認（location ごとの拡張子分離）

`/cgi_py` は `.py` のみ CGI として動き、`.php` は静的ファイルとして返る想定です。

```sh
curl -i "http://127.0.0.1:18084/cgi_py/hello.py?only=py"
curl -i "http://127.0.0.1:18084/cgi_py/hello.php"
```

期待:
- 1つ目は `python ok` が含まれる
- 2つ目は CGI 実行されず、`<?php` がそのまま返る

同様に `.php` / `.js` / `.sh` も確認できます。

```sh
curl -i "http://127.0.0.1:18084/cgi_php/hello.php?only=php"
curl -i "http://127.0.0.1:18084/cgi_node/hello.js?only=node"
curl -i "http://127.0.0.1:18084/cgi_bash/hello.sh?only=bash"
```
