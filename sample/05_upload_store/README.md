# 05_upload_store

`upload_store`（受信した request body をファイルとして保存）と、`client_max_body_size` の
- デフォルト
- server → location への継承
- location による上書き（境界値）

を手動で確認するサンプルです。

このサンプルの upload_store は **POST のみ対応**です。

## 保存ルール（改善後仕様）

- raw（`multipart/form-data` 以外）
	- URL が **ファイル指定**（例: `/upload/foo.bin`）の場合: 指定されたファイル名に保存（既存挙動互換で上書き）
	- URL が **ディレクトリ指定**（例: `/upload/`）の場合: `YYYYMMDDHHMMSS_uploaded(_N).bin` のような名前で保存
- `multipart/form-data`
	- file part ごとに保存（複数可）
	- `filename` と `Content-Type` から保存ファイル名/拡張子を決め、同名衝突は `_N` でユニーク化

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

### スモークテスト（自動）

サンプル全体のスモークテスト（[sample/smoke_test.sh](../smoke_test.sh)）の中で、
`05_upload_store` は以下を自動検証します。

- **上限制御**
	- raw 2MiB → `413`（保存されない）
	- `multipart/form-data` 2MiB → `413`（保存されない）
- **raw 保存**
	- 512KiB → `201`（保存される・サイズ一致）
	- ディレクトリ宛（`/upload/`）→ `YYYYMMDDHHMMSS_uploaded(_N).bin` で保存
	- 同名ファイルへ2回 POST → 2回とも `201`（上書きされる）
- **multipart/form-data 保存**
	- `filename` + `Content-Type` から拡張子を決定して保存
	- `filename` が空の場合、`YYYYMMDDHHMMSS_uploaded(_N).png` のようなユニーク名で保存
	- `Content-Type` が無い場合、デフォルト拡張子 `bin` を適用
	- リクエスト先URIの末尾名より、ボディ側 `filename`/`Content-Type` を優先
	- 異なる拡張子の複数ファイルを同時にアップロードできる
- **バイナリ耐性**
	- ボディ中に `\0` を含んでも raw/multipart ともに内容が欠けずに保存される
- **POST以外では保存しない**
	- GET/DELETE に body を付けても upload_store されない（ファイル数が増えない）
- **書き込み不可の upload_store**
	- 保存先が書き込み不可の場合、raw（ファイル宛/ディレクトリ宛）/multipart ともに `403`

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
- `multipart/form-data` → `201`（file part の中身が保存される）
- `/upload/`（ディレクトリ宛 raw）→ `201`（timestamp 名で保存される）
- 同名ファイルに再度 POST → `201`（上書きされる）

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
