# sample/

webserv の手動動作確認（`curl`）用のサンプル設定と素材置き場です。

- このプロジェクトの config パーサは **相対パスを CWD（起動時のカレントディレクトリ）基準**で解決します。
  - そのため、基本はリポジトリルート（`webserv/`）で `./webserv sample/.../webserv.conf` を実行してください。

## 収録テストケース

- `01_static/`: 静的ファイル配信（200 + body）
- `02_error_page/`: error_page（400/403/404/405/413/500）のカスタムHTML
- `03_autoindex/`: autoindex（ディレクトリ一覧HTML）
- `04_cgi/`: CGI（python/php/node/bash）
- `05_upload_store/`: upload_store（保存先、client_max_body_size のデフォルト/継承/上書き、境界）

## smoke test

`sample/` 配下のサンプルは、起動→ `curl` の確認をまとめたスモークテストで一括検証できます。

```sh
make -j
bash sample/smoke_test.sh
```

運用ルール: **今後 sample/ 配下にサンプルを追加した場合は、必ず `sample/smoke_test.sh` に確認手順を追記**してください。
