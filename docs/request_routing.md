# httpリクエスト時のサーバルーティング仕様

このドキュメントは、サーバーが HTTP リクエストを受け取ったときに
「どの virtual server（server ブロック相当）を選び」「どの location を選ぶか」を、
request_router 実装の挙動に合わせて定義します。

---

## 1. 入力と出力

### 入力

| 項目 | 型 | 説明 |
| :--- | :--- | :--- |
| HTTP リクエスト | `http::HttpRequest` | パース済みのリクエスト（Request-Target, Host 等） |
| listen endpoint | `IPAddress` + `PortType` | 実際に accept された待受け（サーバ側）IP/Port |

補足:

- Host ヘッダー内の `:port` は server_name 照合の際には無視します（ホスト部のみを使う）。
- listen endpoint は「Host ヘッダーの値」ではなく、サーバーが実際に待ち受けていた IP/Port を使います。

### 出力

`RequestRouter::route()` は `utils::result::Result<LocationRouting>` を返します。

| 状態 | `Result` | `LocationRouting.getHttpStatus()` | `LocationRouting.getNextAction()` | 意味 |
| :--- | :--- | :--- | :--- | :--- |
| ルーティング処理不能 | ERROR | - | - | 設定不整合（listen endpoint に一致する server が無い等） |
| リクエスト不正 | OK | 400 | RESPOND_ERROR | パスが不正、または dot-segments 解決でルート逸脱 |
| ルーティング成功 | OK | 200 | (各 ActionType) | virtual server が選べた（location 未一致の場合は 404 扱い） |

注意:

- `Result` が OK でも、HTTP としては 400/404 などのエラーになり得ます。上位層は `LocationRouting.getHttpStatus()` と `LocationRouting.getNextAction()` を必ず見ます。

---

## 2. Request-Target（パス）と Host の正規化

request_router は、virtual server / location の選択前に Request-Target と Host を正規化します。

### 2.1 パスの前提条件

以下の場合、ルーティング結果は `400 Bad Request` になります（virtual server / location は未選択）。

- Request-Target のパスが空、または `/` で始まらない
- パスに NUL (`\\0`) が含まれる

### 2.2 連続スラッシュの正規化

パス中の連続した `/` は 1 個に正規化します。

例:

	/b//x///y  ->  /b/x/y

### 2.3 dot-segments（`.` / `..`）の解決

RFC 3986 の remove-dot-segments 相当の処理で `.` / `..` を解決します。

- `.` は無視
- `..` は 1 つ前のセグメントを削除
- ルートより上に出ようとした場合（例: `/../secret`）は `400 Bad Request`

末尾が `/` で終わる場合は（`/a/b/` のようなケース）、解決後も末尾 `/` を維持します。

### 2.4 Host の抽出

`Host` ヘッダーの最初の値を使い、`host:port` の形なら `:` 以降（port 部分）を切り捨てて
server_name 照合用の host を得ます。

例:

	Host: example.com:8080  ->  server_name 照合は "example.com"

`Host` が存在しない／空の場合は、空文字として扱います（後述の「デフォルト server 選択」へ）。

---

## 3. virtual server（server ブロック相当）の選択

### 3.1 対象 server の絞り込み（listen endpoint マッチ）

まず、設定されている server の中から、次を満たすものを候補とします。

- port が一致
- host が wildcard（例: `0.0.0.0` 相当）または listen endpoint の IP と一致

1つの server に複数の `listen` が設定されている場合は、**そのうち1つでも**
listen endpoint に一致すれば候補になります（any-match）。

### 3.2 同一 IP:Port に複数 server がある場合の選択順

同一の IP とポートの組み合わせに対して複数 server が候補になる場合、選択は次の優先度です。

1. **Host が空でない** かつ `server_name` に **完全一致**する server（設定ファイル順で最初に一致したもの）
2. それ以外は **デフォルト server**（候補のうち設定ファイル順で最初に現れたもの）

備考:

- `server_name` 照合は **完全一致**です（部分一致やワイルドカードはありません）。
- 大文字小文字の正規化は行いません（文字列として一致判定）。
- `server_name` が未設定（空扱い）の server は、Host による選択対象にはならず
  「デフォルト server」としてのみ選ばれます。

### 3.3 対応する listen endpoint が無い場合

listen endpoint に一致する server が 1 つも無い場合は、`Result` が ERROR になります。

---

## 4. location の選択

virtual server が選ばれた後、その server が持つ location の中から 1 つを選びます。

### 4.1 マッチ方式

location には 2 種類のマッチ方式があります。

| 方式 | 意味 | 例 |
| :--- | :--- | :--- |
| forward（前方一致 / prefix match） | `path` が `pattern` で始まる | pattern=`/images` は `/images/logo.png` に一致 |
| backward（後方一致 / suffix match） | `path` が `pattern` で終わる | pattern=`/cgi-bin` は `/app/cgi-bin` に一致 |

注意:

- forward は「セグメント境界」を見ない単純 prefix です。
  例として、pattern=`/img` は `/images` にも一致します。必要なら `/img/` のように設計します。

### 4.2 最長一致（longest match）

複数の location が一致する場合は、**pattern の文字列長が最長**のものを選びます。

同じ長さの pattern が複数ある場合は、**設定ファイルで先に定義された方**が選ばれます。

### 4.3 location が 1 つも一致しない場合

location が一致しない場合でも、`route()` 自体は OK を返します。

- `LocationRouting.getHttpStatus()` は `404 Not Found`
- `LocationRouting.getNextAction()` は `RESPOND_ERROR`

上位層（Session）は `RESPOND_ERROR` と status を見て、エラーレスポンス（または error_page の内部リダイレクト）を構築します。

---

## 5. 物理パス（ファイルシステム上のパス）の扱い

RequestRouter の責務は「virtual server / location の選択」と「URI の正規化」です。
物理パスの確定やファイル存在確認などの FS アクセスは、基本的に Session 層で行います。

例外（CGI）:

- CGI 拡張子がマッチした場合に限り、ルーティング（`LocationRouting::decideAction_()`）がスクリプト実体を `stat()` し、
	- 不在/regular file でない場合は `404 Not Found`（`RESPOND_ERROR`）を確定させます。
	- ねらい: 「スクリプト不在（404）」と「CGI実行時異常（502/504）」を区別するため。

Session 層は、必要に応じて LocationRouting の API から「安全な物理パス」を取得します。

- 静的ファイル: `LocationRouting::resolvePhysicalPathUnderRootOrError()`
- アップロード保存: `LocationRouting::getUploadContext()`（保存先は `destination_path`）
- CGI: `LocationRouting::getCgiContext()`（実行ファイルと `script_filename`）
- autoindex: `LocationRouting::getAutoIndexContext()`（`directory_path` と `index_candidates`）

補足:

- location が forward（prefix match）の場合、物理解決では「`request_path` から `location.pattern` を除去した残り」を root 配下として辿ります。
- location が backward（suffix match）の場合、`location.pattern` は除去されず、`request_path` 全体が root 配下として扱われます。

---

## 6. セキュリティ・注意点（request_router の責務範囲）

- request_router は URI の dot-segments によるルート逸脱（`/../`）を **400** として弾きます。
- ただし、ドキュメントルート外へのアクセス（symlink 逸脱など）まで request_router 単体では保証しません。
  ファイルアクセス直前に `LocationRouting::resolvePhysicalPathUnderRootOrError()` を用いて
  物理解決ベースで root 配下に留まることを検証します。
- ファイルの存在確認（`stat`）は、`LocationRouting::getPath()` の外で行う想定です。

---

## 7. 具体例

### 例1: 同一 IP:Port に複数 server がある場合（server_name が一致する）

設定（概念）:

	server { listen 0.0.0.0:8080; server_name a.example.com; root /var/www_a; location /a { } }
	server { listen 0.0.0.0:8080; server_name b.example.com; root /var/www_b; location /b { } }

リクエスト:

	GET /b//x/../y HTTP/1.1
	Host: b.example.com

処理の要点:

1. パス正規化: `/b//x/../y` → `/b/y`
2. server 選択: `Host=b.example.com` が一致する server（2つ目）
3. location 選択: `/b` が一致

結果（概念）:

- 選ばれる server: 2つ目（`root=/var/www_b`）
- `LocationRouting.getNextAction()` は `SERVE_STATIC`
- `LocationRouting.getStaticUriPath()` は `/b/y`
- Session 層は `resolvePhysicalPathUnderRootOrError()` を呼び、`/var/www_b/y` 相当の物理パスを得る

### 例2: 同一 IP:Port に複数 server がある場合（server_name が一致しない）

同じ設定で、Host が一致しない場合:

	GET /a/test HTTP/1.1
	Host: unknown.example.com

結果:

- server_name が一致する候補が無いので、デフォルト server（設定順で最初の server）
- `LocationRouting.getStaticUriPath()` は `/a/test`
- Session 層は `resolvePhysicalPathUnderRootOrError()` を呼び、`/var/www_a/test` 相当の物理パスを得る

### 例3: Host ヘッダーが無い（または空）

	GET /a/test HTTP/1.1

結果:

- Host が空扱いなので server_name による選択は行われず、デフォルト server

### 例4: `/../` によるルート逸脱

	GET /../secret HTTP/1.1
	Host: a.example.com

結果:

- dot-segments 解決でルート逸脱 → `400 Bad Request`
- virtual server / location は未選択

### 例5: location の最長一致（同じ長さなら先勝ち）

設定（概念）:

	server {
	  listen 0.0.0.0:8080;
	  root /;
	  location / { root /var/www; }
	  location /images { root /var/images; }
	}

リクエスト:

	GET /images/logo.png HTTP/1.1
	Host: example.com

結果:

- `/` と `/images` が一致するが、`/images` の方が長いので採用
- 絶対パスは `/var/images/logo.png`

### 例6: 1つの server に複数 listen（ワイルドカード含む）

設定（概念）:

	server {
	  listen 0.0.0.0:8080;
	  listen 127.0.0.1:8080;
	  server_name a.example.com;
	  root /var/www_a;
	  location / { }
	}
	server {
	  listen 127.0.0.1:8080;
	  server_name b.example.com;
	  root /var/www_b;
	  location / { }
	}

ケースA: 接続先（listen endpoint）が `127.0.0.1:8080` のとき

	GET /index.html HTTP/1.1
	Host: b.example.com

- 候補: 1つ目（`127.0.0.1:8080` で一致）＋ 2つ目（`127.0.0.1:8080` で一致）
- server_name に完全一致するのは 2つ目 → **2つ目が選ばれる**

ケースB: 接続先（listen endpoint）が `192.168.0.10:8080` のとき

	GET /index.html HTTP/1.1
	Host: b.example.com

- 候補: 1つ目のみ（`0.0.0.0:8080` が wildcard として一致）
- 2つ目は `127.0.0.1:8080` しか listen していないため候補外
- よって server_name が一致していても選べず、**1つ目（デフォルト）が選ばれる**

重要:

- server_name による選択は「listen endpoint に一致した候補」の中でのみ行われます。
