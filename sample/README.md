# sample/

webserv の手動動作確認（`curl`）用のサンプル設定と素材置き場です。

- このプロジェクトの config パーサは **相対パスを CWD（起動時のカレントディレクトリ）基準**で解決します。
  - そのため、基本はリポジトリルート（`webserv/`）で `./webserv sample/.../webserv.conf` を実行してください。

## 収録テストケース

- `01_static/`: 静的ファイル配信（200 + body）
- `02_error_page/`: error_page（400/403/404/405/413/500）のカスタムHTML
