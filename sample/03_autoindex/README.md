# 03_autoindex

`autoindex on;` により、ディレクトリの一覧（HTML）を返すサンプルです。

## 起動

```sh
./webserv sample/03_autoindex/webserv.conf
```

- listen: `127.0.0.1:18083`
- root: `sample/03_autoindex/www`

## 確認

`/dir/` にアクセスすると、ディレクトリ一覧ページが返ります。

```sh
curl -v http://127.0.0.1:18083/dir/
```

期待:
- `HTTP/1.1 200`
- `Content-Type: text/html`
- body に `Index of /dir/` が含まれる
- `a.txt` と `sub/` のリンクが含まれる

`sub/` 側も同様に確認できます。

```sh
curl -v http://127.0.0.1:18083/dir/sub/
```

（このサンプルでは、`www/dir/a.txt` と `www/dir/sub/b.txt` が配置されています。）
