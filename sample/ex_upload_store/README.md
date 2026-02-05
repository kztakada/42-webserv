# ex_upload_store（手動専用）

`upload_store` を **ブラウザのドラッグ＆ドロップ** で手動確認するための extra サンプルです。

- `smoke_test.sh` には載せません
- 静的なアップロードサイト（HTML/CSS/JS）を配信します
- 画像をドロップすると、JS が **同一オリジンの相対URL** に `POST` でアップロードします
  - ブラウザの `FormData` を使って `multipart/form-data` で送ります

## 起動

このプロジェクトの config パーサは **相対パスを CWD 基準**で解決します。
リポジトリルート（`webserv/`）で起動してください。

```sh
./webserv sample/ex_upload_store/webserv.conf
```

- listen: `127.0.0.1:18090`
- 静的サイト: `http://127.0.0.1:18090/`
- upload 先 location: `/upload`（POST のみ）
- upload_store: `sample/ex_upload_store/store/`

## テスト方法（ブラウザ）

1. ブラウザで `http://127.0.0.1:18090/` を開く
2. 画像ファイル（jpg/png等）をページのドロップ領域へドラッグ＆ドロップ
3. 右側のログに `status: 201 ...` が出ることを確認
4. 別ターミナルで保存ファイルを確認

```sh
ls -l sample/ex_upload_store/store
```

保存ファイル名は JS 側で `timestamp_originalname` のように生成します。

## 失敗したときの見どころ

- `404` になる: `location /upload` の設定やパスがズレています
  - [sample/ex_upload_store/www/app.js](sample/ex_upload_store/www/app.js) の `UPLOAD_BASE_PATH` が `/upload` になっているか確認
- `405` になる: `location /upload` の `allow_methods POST;` を確認
- `413` になる: `client_max_body_size` を増やす（conf 側）
- `500` になる: 保存先ディレクトリの権限/パス不整合などを確認

## 後片付け

成功したアップロードは `sample/ex_upload_store/store/` に残ります。必要なら削除してください。
