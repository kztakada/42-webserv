# 05_upload_store

`upload_store`（受信した request body をファイルとして保存）と、`client_max_body_size` の
- デフォルト
- server → location への継承
- location による上書き（境界値）

を手動で確認するサンプルです。

このサンプルの upload_store は **POST のみ対応**です。

## 起動

このプロジェクトの config パーサは **相対パスを CWD（起動時のカレントディレクトリ）基準**で解決します。
リポジトリルート（`webserv/`）で起動してください。

### デフォルト上限（1MB想定）

```sh
./webserv sample/05_upload_store/webserv_default_limit.conf
```

- listen: `127.0.0.1:18085`
- upload 先: `sample/05_upload_store/store_default/`

### server の上限を継承（12M → 10M OK）

```sh
./webserv sample/05_upload_store/webserv_inherit_server_limit.conf
```

- listen: `127.0.0.1:18086`
- upload 先: `sample/05_upload_store/store_inherit/`

### location で上書き（1MiB境界 / 20M）

```sh
./webserv sample/05_upload_store/webserv_location_override.conf
```

- listen: `127.0.0.1:18087`
- upload 先: `sample/05_upload_store/store_override/`

## 画像（手動ケースのみ）

手動テストでは、ディレクトリ内に **全面ホワイトの JPEG** を生成して、それを元にサイズを調整した payload を作ってアップロードします。

生成（1回だけでOK）:

```sh
bash sample/05_upload_store/gen_white_jpg.sh
```

- 生成先: `sample/05_upload_store/fixtures/white.jpg`
- ImageMagick 等は不要で、`base64` と `python3` だけで生成します。

## 確認

### まず GET が返ること（任意）

どの conf でも `root` は `sample/05_upload_store/www` を指します。

```sh
curl -v http://127.0.0.1:18085/
```

### upload_store + client_max_body_size の確認（curl スクリプト）

別ターミナルで webserv を起動した状態で、以下を実行します。

#### デフォルト上限（1MB想定）

```sh
bash sample/05_upload_store/curl_upload_store.sh default
```

期待:
- 2MiB → `413`（保存されない）
- 512KiB → `201`（保存される、サイズ一致）

#### 継承（server: 12M を location が継承）

```sh
bash sample/05_upload_store/curl_upload_store.sh inherit
```

期待:
- 10MiB → `201`（保存される、サイズ一致）

#### location 上書き（境界値）

```sh
bash sample/05_upload_store/curl_upload_store.sh override
```

期待:
- 1,048,576 bytes (= 1MiB) → `201`
- 1,048,577 bytes (= 1MiB + 1) → `413`（保存されない）
- 別 location（20M）で 10MiB → `201`

## 後片付け

- 成功したアップロードは `store_*` にファイルが残ります。
- 生成したホワイト JPEG は `fixtures/white.jpg` に残ります（必要なければ削除してください）。
