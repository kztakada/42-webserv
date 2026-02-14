# RFC9112 / RFC3875 テストケース一覧（実装前）

本ファイルは、課題要件（HTTP/1.1: RFC 9112、CGI: RFC 3875）を満たすかを検証するためのテスト項目一覧です。**実装は行わず、テスト内容の列挙のみ**とします。

---

## 成功してほしいテストケース（Must/Should）

### HTTP/1.1 基本構文・メッセージ
- リクエストライン: `GET / HTTP/1.1` を受理できる
- リクエストライン: メソッド/ターゲット/バージョンが単一SP区切りで解析できる
- `HTTP/1.1` バージョンを正しく認識できる
- ヘッダーフィールド: `field-name: field-value` を解析できる（OWSの許容）
- ヘッダーの大文字小文字非依存で扱える（`Host`/`host` 等）
- ヘッダーフィールドの複数行連結（obsolete folding）は**拒否**または明示的に非対応（RFC9112準拠の処理方針を明確化）
- 空行でヘッダー終端を認識できる
- 先頭行のCRLF終端を受理できる

### 必須ヘッダー
- `Host` ヘッダー必須（HTTP/1.1）を満たす
- `Host` がポート付き（`example.com:8080`）でも正しく受理できる
- `Host` がIPリテラル（`127.0.0.1` / `[::1]`）でも正しく受理できる

### 文字セット・バイト許容
- ヘッダーフィールド値に `obs-text`（0x80-0xFF）が含まれても扱える（または明示的に拒否）
- Request Target に `*`（`OPTIONS * HTTP/1.1`）を受理できる

### メソッド
- `GET` を正常に処理できる
- `HEAD` を正常に処理できる（ボディなし）
- `POST` を正常に処理できる（ボディあり）
- `DELETE` を正常に処理できる
- `OPTIONS` を正常に処理できる（Allowヘッダー）

### メッセージボディ
- `Content-Length` に基づいてボディを読み込める
- `Content-Length: 0` を正しく扱える
- `Transfer-Encoding: chunked` を正しく処理できる
- `chunked` と `Content-Length` が同時にある場合の優先順位（chunked優先）を満たす
- チャンクサイズが16進数で解釈される
- 最終チャンク `0\r\n` で終端できる
- チャンク拡張（`;ext=value`）を無視して処理できる
- トレーラーヘッダーを受理できる（もしくは明示的に拒否）

### 接続管理
- `Connection: close` を尊重して接続を閉じる
- `Connection: keep-alive` で持続接続を維持できる
- 1接続で複数リクエストを順序通り処理できる（パイプライン）
- パイプライン時に順序を保持してレスポンスできる

### ステータスコード
- `200 OK` で正常応答を返せる
- `204 No Content` を適切に返せる（ボディなし）
- `301/302` リダイレクトを返せる
- `304 Not Modified` を条件付きリクエストに応じて返せる（ETag/If-Modified-Since）
- `400 Bad Request` を返せる
- `404 Not Found` を返せる
- `405 Method Not Allowed` を返せる（Allowヘッダー必須）
- `411 Length Required` を返せる（ボディ必須時にCL未指定）
- `413 Payload Too Large` を返せる
- `414 URI Too Long` を返せる
- `500 Internal Server Error` を返せる
- `501 Not Implemented` を返せる
- `505 HTTP Version Not Supported` を返せる

### ヘッダ生成
- `Date` ヘッダーを返せる
- `Server` ヘッダーを返せる（任意だが一般的）
- `Content-Type` を拡張子から判定できる
- `Content-Length` を正しく返せる（chunkedでない場合）

### ルーティング / パス解決
- ルート配下でのパス解決が正しい
- パストラバーサル（`../`）を拒否できる
- 末尾 `/` 付きのディレクトリに対して `index` が有効
- `autoindex` ON でディレクトリ一覧が返る
- location の優先順位が最長一致（Longest Prefix Match）になる（例: `/` と `/images/` がある時に `/images/logo.png` は `/images/` が選ばれる）

### 設定ファイル（Configuration：課題要件）
- 設定ファイルで指定した `client_max_body_size` が反映され、超過時に `413 Payload Too Large` が返る
- ルート（location）ごとの許可メソッド設定が反映される（例: GETのみ許可しPOSTで `405 Method Not Allowed`）
- `error_page`（カスタムエラーページ）が設定通りに表示される（404/500 など）
- `error_page` が未設定の場合に、サーバーが用意したデフォルトエラーページが返る
- アップロード保存先（upload store）の設定が反映され、アップロードされたファイルが指定ディレクトリに保存される
- リダイレクト設定が反映され、指定URLへの `301/302` が正しく返る（`Location` ヘッダ含む）

### 多重化・非ブロッキング I/O（課題要件）
- 複数ポート（例: 8080 と 8081）で同時に待受できる
- ポートごとに異なる server block 設定が適用され、異なるコンテンツを配信できる
- スロークライアント（Slowloris）相当の非常に遅い送信があっても、他接続の処理がブロックされず進む
- 多数同時接続時にクラッシュせず、応答の欠落/停止が起きない（負荷テスト）
- CGI のパイプ（stdin/stdout）I/O も同一の I/O 多重化ループで管理され、CGI の read/write 待ちでサーバー全体がブロックしない

### シグナル/クリーン終了（課題要件）
- `SIGINT`（Ctrl+C）などで終了させた時に、即座にソケットをクローズして終了できる
- 終了直後に再起動してもポートを再バインドできる（`Address already in use` にならない）

### CGI（RFC 3875）
- CGI起動時に環境変数が正しく設定される
- `REQUEST_METHOD` が正しい
- `SCRIPT_NAME` が正しい
- `PATH_INFO` が正しい
- `PATH_TRANSLATED` が `PATH_INFO` と root の対応関係に基づいて正しく設定される（`PATH_INFO` がある場合）
- `QUERY_STRING` が正しい
- `CONTENT_TYPE` が正しい
- `CONTENT_LENGTH` が正しい
- `SERVER_PROTOCOL` が正しい（`HTTP/1.1`）
- `SERVER_NAME` / `SERVER_PORT` が正しい
- `REMOTE_ADDR` が正しい
- `HTTP_*` 形式でリクエストヘッダが渡る
- CGIが `Status:` ヘッダーを返した場合にHTTPステータスに反映される
- CGIが `Content-Type` を返した場合にそのまま採用される
- CGIレスポンスのヘッダーとボディの区切りを正しく扱える（空行）
- CGIがボディのみ返した場合のデフォルト `Content-Type` を補う
- CGI標準出力がHTTPレスポンスに正しく変換される
- CGIが標準エラーに出力してもサーバーが落ちない

### CGI（課題PDF由来の細部）
- `Transfer-Encoding: chunked` のリクエストボディをサーバー側で un-chunk してから CGI の stdin に渡せる
- CGI が `Content-Length` を返さない場合、CGI プロセス終了（EOF）をレスポンス終端として正しく扱える
- CGI が相対パスでファイルアクセスする場合に備え、適切な作業ディレクトリ（working directory）で実行される

### タイムアウト・リソース
- CGIのタイムアウトを超えたら強制終了できる
- クライアントの読み込みタイムアウトが効く
- 書き込みタイムアウトが効く

---

## 失敗してほしいテストケース（拒否/エラー）

### HTTP/1.1 構文エラー
- リクエストラインが3要素でない（`GET /` など）
- バージョンが `HTTP/1.0` または未知（`HTTP/2.0`）
- CRLFではなくLFのみのリクエスト（要件により拒否）
- ヘッダーフィールド名に空白や制御文字が含まれる
- ヘッダーに `:` が無い
- `Host` ヘッダー欠如
- `Host` に無効な形式（空、複数、制御文字）
- `Content-Length` が数値でない
- `Content-Length` が負数
- `Content-Length` が複数かつ不一致
- `Transfer-Encoding` が `chunked` 以外のみ（未対応TEなら拒否）
- `Transfer-Encoding` と `Content-Length` の両方が矛盾

### メソッド/ルーティング
- 許可されていないメソッドへのアクセス（`405 Method Not Allowed`）
- 未実装メソッド（`TRACE`, `CONNECT`）へのアクセス（`501 Not Implemented`）
- パストラバーサル（`/../`）
- URIが長すぎる（`414 URI Too Long`）
- `/` と `/images/` のようなネストされた location がある場合に、最長一致が守られず誤った location が選ばれる（失敗として検出）

### ボディ・チャンク
- チャンクサイズが16進数でない
- チャンクサイズ行の後にCRLFがない
- 最終チャンクの後の終端が不正
- チャンク拡張の構文が不正
- `Content-Length` より短い/長いボディ送信

### 接続/持続
- `Connection: close` でも接続が閉じない
- パイプラインでレスポンス順が入れ替わる
- 1つのリクエスト失敗で後続処理が破綻する

### CGI I/O 多重化（課題要件）
- CGI へ巨大入力を送信中（stdin write）に、別クライアントの静的ファイル応答が極端に遅延する（CGI I/O がブロックしている）
- CGI が巨大出力を生成中（stdout read）に、別クライアント処理が停止する（CGI I/O がブロックしている）

### クライアント突然切断（課題要件）
- リクエストボディ送信途中にクライアントが切断すると、サーバーが `SIGPIPE` 等でクラッシュする
- レスポンス送信途中にクライアントが切断すると、FD がリークする/セッションが残留する

### ステータス/ヘッダー
- `HEAD` でボディが返ってしまう
- `204` / `304` でボディが返ってしまう
- `405` で `Allow` が無い
- `Date` ヘッダーが欠落（方針次第だが一般的には必要）

### CGI（RFC 3875）
- CGI環境変数不足（`REQUEST_METHOD` など）
- `CONTENT_LENGTH` がボディ長と不一致
- CGIヘッダーが不正（空行無し）
- CGIが `Status:` を返しても反映されない
- CGIが `Location:` のみ返した時の処理が誤り（ローカル/外部）
- CGIが巨大出力でサーバーが落ちる
- CGIプロセスが終了しないのにレスポンスが返る
- CGIが異常終了しても `500` にならない
- `PATH_TRANSLATED` が `PATH_INFO` と整合しない（誤った物理パスになる/未設定になる）

### 設定ファイル/起動の堅牢性（課題要件）
- 設定ファイルに構文エラーがある場合、サーバーがクラッシュせずにエラーを出力して終了する
- 設定ファイルで未知ディレクティブ/不正値（負数、非数、無効ポート等）がある場合に安全に失敗する
- server/location のネストや必須要素不足など、設定の構造不正で安全に失敗する

### 不正なリソースアクセス（堅牢性）
- 存在しない CGI スクリプトにアクセスした場合に適切なエラーコードを返す（例: `404` または `500` の方針に従う）
- 実行権限のない CGI スクリプトにアクセスした場合に適切なエラーコードを返す（例: `403` または `500` の方針に従う）

### シグナル/クリーン終了
- `SIGINT` で停止後、再起動時に `Address already in use` が出て起動できない
- 停止時に CGI 子プロセスが残留してゾンビ化する（`waitpid` 不備の疑い）

---

## 補助観点（任意だが推奨）

- ログに不正リクエストの理由が分かる
- エラーページが設定で切り替え可能
- 大量同時接続での安定性（イベント駆動）
- 既存のnginx互換設定の解釈（listen/server_name/root/location）

---

## 具体的なテスト手順例（コマンド例）

本節は「実装」ではなく、手元検証を再現しやすくするための手順例です（環境によりコマンドは読み替え）。

### `client_max_body_size`（413 の具体的な検証方法）
- 前提: `client_max_body_size` を小さめ（例: 1KB）に設定した location を用意する
- 期待: 上限以下は `200`、上限超過は `413 Payload Too Large`
- 例（Content-Length あり / curl）
	- 1KB 未満: `printf 'a%.0s' {1..512} | curl -v --http1.1 -H 'Content-Type: text/plain' --data-binary @- http://127.0.0.1:PORT/upload`
	- 1KB 超過: `printf 'a%.0s' {1..2048} | curl -v --http1.1 -H 'Content-Type: text/plain' --data-binary @- http://127.0.0.1:PORT/upload`
- 例（chunked で超過を検証したい場合 / nc）
	- `nc 127.0.0.1 PORT` で接続し、`Transfer-Encoding: chunked` のリクエストを手で送る
	- 複数チャンクの**合計**が上限超過になるようにして送る
	- 期待: サーバーが un-chunk 後の実サイズで判定し、超過時に `413` を返す

### 複数ポート同時リスニング（手順）
- 前提: 2つの server block を作り、それぞれ `listen 8080;` / `listen 8081;` とし、root か返すファイルを変える
- 手順
	- サーバー起動後、`curl -sS http://127.0.0.1:8080/` と `curl -sS http://127.0.0.1:8081/` を実行
	- 期待: 両方が応答し、内容がそれぞれの server block に対応して異なる
	- 並列性確認: 片方に遅いクライアント（`curl --limit-rate 10B --data-binary @bigfile ...` 等）を作っても、もう片方の GET が即応答する

### CGI 実行時のディレクトリ（Working Directory）で注意すべき点
- 注意点
	- サーバープロセス全体で `chdir()` すると、単一プロセス構成では他リクエストにも影響するため危険
	- 典型解: `fork()` 後の CGI 子プロセス側でのみ `chdir()` してから `execve()` する
- 検証方法（例）
	- CGI が `pwd` を出す/相対パスで `./fixture.txt` を読むようなスクリプトを用意
	- 期待: その相対パスが意図通り解決される（かつ、別リクエストのファイル解決に副作用が出ない）

---

## RFC別まとめ（補足）

> ここに列挙した項目は、実装状況ではなく課題要件（RFC 9112 / RFC 3875）を基準に整理したもの。

### RFC9112: メッセージボディと長さ判定
- `Content-Length` が単一で正しい場合に正常処理
- `Content-Length` が複数で同値なら受理、異なるなら拒否（400）
- `Transfer-Encoding` と `Content-Length` が同時にある場合は `Transfer-Encoding` を優先
- `chunked` が最後にある場合のみ受理（他のTEが先行する場合は拒否）

### RFC9112: 改行・空白・ヘッダ構文
- CRLF が行終端として正しく扱われる
- 空行（CRLF CRLF）でヘッダ終端を認識する
- フィールド名の不正文字（制御文字、スペース等）を拒否
- obsolete line folding（obs-fold）は受理せず 400 を返す

### RFC9112: ステータスコード
- 200/204/400/403/404/405/413/414/431/500/501/502/503 を返せる
- 405 の場合に `Allow` ヘッダが正しく付く
- HEAD でボディを送らずヘッダのみ返却される

### RFC9112: リクエストターゲット
- origin-form（`/path?query`）が正しく解釈される
- absolute-form（プロキシ用）を拒否または適切に処理
- authority-form（CONNECT）は拒否（未対応なら 405/501）
- asterisk-form（`OPTIONS *`）に対する適切な応答
- URI の `?query` を保持し、パスと分離できる
- パス正規化の前後で `..` / `.` を正しく解釈できる

### RFC3875: CGI 1.1 メタ変数
- `REQUEST_METHOD` が正しく設定される
- `SCRIPT_NAME` / `PATH_INFO` / `PATH_TRANSLATED` の整合が正しい
- `QUERY_STRING` がURLデコードせずに設定される
- `CONTENT_TYPE` / `CONTENT_LENGTH` が正しく設定される
- `SERVER_PROTOCOL` が `HTTP/1.1` になる
- `SERVER_NAME` / `SERVER_PORT` が正しく設定される
- `REMOTE_ADDR` が正しく設定される
- `HTTP_*` 変数（ヘッダ転写）が正しく設定される
- `SERVER_SOFTWARE` が実装の識別子として設定される
- `GATEWAY_INTERFACE` が `CGI/1.1` になる

### RFC3875: CGI 入出力
- リクエストボディが stdin に正しく渡る
- CGI の stdout からヘッダを受け取りHTTPレスポンスへ変換できる
- CGI の stderr をログ出力に回せる（機能要件次第）
- CGI が `Status:` ヘッダを返した場合にそれをHTTPステータスへ反映
- `Location:` ヘッダを返した場合に適切なリダイレクト処理
- CGI が `Content-Type` を返した場合にレスポンスに反映
- CGI が `Content-Length` を返した場合にボディ長を正しく扱う

## RFC別: 失敗してほしいテストケース

### RFC9112: メッセージ構文違反
- HTTP/1.1 で `Host` ヘッダが無い要求（400）
- 要求行のHTTPバージョンが `HTTP/1.0` / 不明値（400/505）
- ヘッダ行が `field-name: value` 形式でない（400）
- フィールド名に空白や制御文字を含む（400）
- obs-fold を含むヘッダ（400）
- `Content-Length` が不正数値（400）
- `Content-Length` が負数（400）
- `Content-Length` が複数で不一致（400）
- `Transfer-Encoding` と `Content-Length` の矛盾（400）
- `Transfer-Encoding` の最後が `chunked` でない（400）
- chunk サイズが16進として不正（400）
- chunk 本文が宣言サイズと不一致（400）
- 不明な `Transfer-Encoding` の指定（400/501）
- `Connection` ヘッダの値が不正形式（400）
- ヘッダ数やヘッダサイズが上限超過（431）

### RFC9112: リクエストターゲット不正
- origin-form 以外（absolute-form/authority-form/asterisk-form）で未対応の場合
- リクエストターゲットが空（400）
- パス中に不正な制御文字が含まれる（400）
- 正規化後にルート外へ出るパス（403）
- 非許可メソッド（405）

### RFC9112: メッセージボディ不整合
- `Content-Length` 付きでボディが短い/長い（400）
- `Expect: 100-continue` に対して本文を待たずに拒否応答しない（失敗として検出）
- `Expect` に未知のトークンがある場合に 417 を返さない（失敗として検出）

### RFC9112: 応答不整合
- HEAD でボディが返ってくる（失敗）
- 204/304 でボディが返ってくる（失敗）
- 405 で `Allow` が無い（失敗）

### RFC3875: CGI メタ変数不整合
- `REQUEST_METHOD` が空/不一致（失敗）
- `QUERY_STRING` がデコード済みで渡される（失敗）
- `CONTENT_LENGTH` と実際の長さが不一致（失敗）
- `SERVER_PROTOCOL` が `HTTP/1.1` 以外（失敗）
- `HTTP_*` 変数が欠落している（失敗）

### RFC3875: CGI 出力不整合
- CGI の stdout に `Content-Type` が無い場合の扱いが不正（失敗）
- CGI が不正なヘッダ行を出力したのに200で返す（失敗）
- CGI がヘッダ終端の空行を出力しないのにボディを解釈する（失敗）
- CGI が `Status:` を返しているのに無視される（失敗）
