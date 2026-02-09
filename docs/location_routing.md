# LocationRouting 仕様（Session 実装者向け）

このドキュメントは、RequestRouter が返す `server::LocationRouting` を Session 層でどう解釈し、
どの API をどの順序で使えばよいかを、実装（`srcs/server/request_router/location_routing.cpp`）に合わせてまとめます。

本プロジェクトの設計上、**LocationRouting は「次に取るべきアクションの決定」と「必要情報の提供」**が責務です。
ファイルの存在確認（`stat()`）や読み書き（`open/read/write`）などの FS アクセスの主体は Session 層です。

例外（CGI）:

- CGI 拡張子がマッチした場合に限り、`LocationRouting::decideAction_()` がスクリプト実体を `stat()` し、
  - 不在/regular file でない場合は `404 Not Found`（`RESPOND_ERROR`）を確定させます。
  - ねらい: 「スクリプト不在（404）」と「CGI実行時異常（502/504）」を区別するため。

---

## 1. 入口と前提

RequestRouter は `RequestRouter::route(const http::HttpRequest&, ...)` で `LocationRouting` を返します。

Session 層は、基本的に次の2つを最初に見て分岐します。

- `LocationRouting::getHttpStatus()`
- `LocationRouting::getNextAction()`

`Result<LocationRouting>` 自体が ERROR になるのは「listen endpoint に一致する virtual server がない」などの内部扱いエラーです。
一方、`Result` が OK でも、HTTP として 400/404/405/413 などのエラー status になり得ます。

---

## 2. ActionType 一覧と対応 getter

`LocationRouting::getNextAction()` が返す `ActionType` は以下です。

| ActionType | Session が取るべき行動 | 主に使う getter |
| :--- | :--- | :--- |
| `SERVE_STATIC` | 静的ファイルとして応答（index 探索も含む） | `getStaticUriPath()`, `getAutoIndexContext()`, `resolvePhysicalPathUnderRootOrError()` |
| `SERVE_AUTOINDEX` | ディレクトリ一覧（autoindex）を生成して応答 | `getAutoIndexContext()` |
| `RUN_CGI` | CGI を起動して応答 | `getCgiContext()` |
| `STORE_BODY` | リクエストボディをファイルへ保存 | `getUploadContext()` |
| `REDIRECT_EXTERNAL` | 外部リダイレクト（Location ヘッダに URL） | `getRedirectLocation()` |
| `REDIRECT_INTERNAL` | 内部リダイレクト（別 URI を再ルーティング） | `getRedirectLocation()`, `getInternalRedirectRequest()` |
| `RESPOND_ERROR` | エラーレスポンスを返す（または error_page の内部リダイレクト） | `getHttpStatus()`, `getDefaultErrorPageBody()` |

重要: getter は **action でガード**されています。
例えば `getCgiContext()` は `RUN_CGI` のときだけ OK を返し、それ以外は `Result(ERROR, ...)` になります。

---

## 3. アクション決定の優先順位（実装準拠）

`LocationRouting` の action は初期化時に `decideAction_()` で決定されます。優先順位は概ね以下です。

1. ルーティング status がエラー（例: 400/405/413 など）
2. location 未一致（404 扱い）
3. `return`（リダイレクト）
4. `client_max_body_size`（Content-Length がある場合のみ早期判定）
5. `allow_methods`（未許可なら 405）
6. `upload_store` + (POST/PUT) → `STORE_BODY`
7. ディレクトリ URI + autoindex 有効 + index 候補なし → `SERVE_AUTOINDEX`
8. CGI 拡張子マッチ → `RUN_CGI`
9. それ以外 → `SERVE_STATIC`

補足:

- `client_max_body_size` は **Content-Length がある場合のみ**初期化時に判定します。
	chunked/不明サイズは Session 側で受信しながら enforce します。

---

## 4. `RESPOND_ERROR` と error_page

### 4.1 代表ケース

- RequestRouter の段階で不正（パスが `/` で始まらない、NUL 含む、dot-segments で root 逸脱など）: `400` + `RESPOND_ERROR`
- location 未一致: `404` + `RESPOND_ERROR`
- allow_methods で拒否: `405` + `RESPOND_ERROR`
- `client_max_body_size` 超過（Content-Length で判明した場合）: `413` + `RESPOND_ERROR`

### 4.2 error_page の扱い

`LocationRouting` はエラー時に `error_page` を参照し、該当設定があれば「内部リダイレクト」に切り替えることがあります。

- error_page の値が **`/` で始まる**場合: `REDIRECT_INTERNAL` になり、`getRedirectLocation()` から URI が取れます。
- error_page の値が **外部 URL** の場合: 現実装では採用せず、`RESPOND_ERROR` のままフォールバックします。

Session 側では以下の流れになります。

- `getNextAction()==REDIRECT_INTERNAL` なら `getInternalRedirectRequest()` を作って **再度 RequestRouter に流す**
- `getNextAction()==RESPOND_ERROR` なら、ボディは `LocationRouting::getDefaultErrorPageBody(status)` などで生成する

---

## 5. `REDIRECT_EXTERNAL` / `REDIRECT_INTERNAL`

### 5.1 `return` によるリダイレクト

location に `return`（redirect_url）が設定されている場合、優先的にリダイレクトになります。

- redirect 先が `/` で始まる: `REDIRECT_INTERNAL`
- それ以外: `REDIRECT_EXTERNAL`

取得 API:

- `getRedirectLocation()`
- `REDIRECT_INTERNAL` の場合は `getInternalRedirectRequest()` で次の内部リクエスト（GET）を生成できます

`getInternalRedirectRequest()` は、HTTP バージョン（minor）や Host を可能な範囲で引き継いだ GET リクエストを組み立てます。

---

## 6. `SERVE_STATIC` と物理パス解決

`SERVE_STATIC` の場合、Session は次の手順で処理するのが基本です。

1. `getStaticUriPath()` で「正規化済みの URI」を取得
2. `getAutoIndexContext()` を（必要なら）呼び、ディレクトリ URI の場合に index 候補を作る
3. ファイルアクセス直前に `resolvePhysicalPathUnderRootOrError()` を呼び、**symlink 逸脱を含む root 逸脱を検知**
4. `stat/open/read` などで実ファイルを読み、レスポンスを生成

`resolvePhysicalPathUnderRootOrError()` は location の `root` を基準に、
`request_path` から location pattern を取り除いたパス（backward match の場合は取り除かれない）を辿り、
symlink による root 逸脱を検知した場合は ERROR を返します。

### 6.1 `resolvePhysicalPathUnderRootOrError()` の使い方まとめ（Q&A）

#### Q1. OK のとき、返り値（PhysicalPath）の中身は使わずに `getStaticUriPath()` のパスを使う？

使いません。**`getStaticUriPath()` は HTTP 上の URI パス**であり、`stat/open/readdir` に渡すものではありません。
`resolvePhysicalPathUnderRootOrError()` が OK のときは、返ってきた `PhysicalPath`（`.str()` で絶対パス）を FS アクセスに使います。

典型フロー（概念）:

```cpp
if (routing.getNextAction() == SERVE_STATIC)
{
    // URI（HTTP上のパス）
    Result<std::string> uri = routing.getStaticUriPath();

    // 物理パス（FSアクセスに使う）
    Result<utils::path::PhysicalPath> p =
        routing.resolvePhysicalPathUnderRootOrError();

    // stat/open/read は p.unwrap().str() に対して行う
}
```

#### Q2. `allow_nonexistent_leaf` を取る場合と取らない場合の違いと、具体的シーンは？

`allow_nonexistent_leaf` は、**末尾要素（leaf）が存在しなくても OK にするか**の違いです。

- `resolvePhysicalPathUnderRootOrError()`（引数なし）
  - `allow_nonexistent_leaf=false` 相当
  - 末尾まで `stat()` で辿れることを要求する（存在しない leaf は ERROR）
- `resolvePhysicalPathUnderRootOrError(true)`
  - 末尾だけは未存在でも OK
  - ただし、**親ディレクトリが root 配下に留まること（symlink 逸脱が無いこと）**は物理的に検証した上で、leaf の `PhysicalPath` を返す

使い分けの目安:

- 読み取り（通常の静的配信）: 基本は引数なし（`false`）
  - 例: `GET /images/logo.png` は「読みに行くファイルが存在している」ことが自然
- 作成の可能性があるパス（アップロード/PUT 等）: `true`
  - 例: まだ存在しない `new.txt` を作って保存したい場合、leaf が未存在なのは正常

補足: `STORE_BODY` の保存先は `getUploadContext()` が返す `destination_path` であり、内部で「末尾未存在 OK」の方針で解決しています。

#### Q3. なぜ STATIC だけが `resolvePhysicalPathUnderRootOrError()` を分離している？ `getStaticUriPath()` に隠蔽すべきでは？

`getStaticUriPath()` と `resolvePhysicalPathUnderRootOrError()` は返す値が別物（URI と物理パス）で、かつ後者は FS に触るため、API として分離しています。

- `getStaticUriPath()`
  - 返すもの: **URI（HTTP上のパス）**
  - 性質: 軽量な getter（ルーティング結果の参照）
- `resolvePhysicalPathUnderRootOrError()`
  - 返すもの: **物理パス（正規化済み絶対パス）**
  - 性質: `chdir/stat` を伴う重い処理。symlink による root 逸脱を検知するため「FS アクセス直前に明示的に呼ぶ」ほうが意図が明確

STATIC は「まず URI を受け取り、`stat` 結果（dir/file/不存在）に応じて index 探索や autoindex 等の追加判断が入る」ケースが多いため、
Session が FS を見に行くタイミングを制御できるように分離しています。

#### Q4. `resolvePhysicalPathUnderRootOrError()` と `getStaticUriPath()` の返り値はどう違う？具体例は？

例（forward / prefix match の location を想定）:

- 設定（概念）
  - `location.pattern = "/images"`
  - `location.root = "/var/www_images"`
- リクエスト（正規化済み request path）
  - `GET /images/logo.png`

このとき:

- `getStaticUriPath()` → `"/images/logo.png"`（URI）
- `resolvePhysicalPathUnderRootOrError()` → `"/var/www_images/logo.png"`（物理パス）

補足（backward / suffix match の場合）:

backward match のときは「pattern を除去しない」ため、物理解決は request path 全体を root 配下として扱います。

#### Q5. 「SERVE_STATIC のときだけ使う」なら、`SERVE_AUTOINDEX` や `RUN_CGI` はアクセスするパスのチェックが不要？

不要ではありません。
ただし、`SERVE_AUTOINDEX` と `RUN_CGI` は専用 Context を返す getter の中で、すでに root 配下としての物理解決（symlink 逸脱検知）を行っています。

- `SERVE_AUTOINDEX` / `SERVE_STATIC`: `getAutoIndexContext()` が `directory_path`（物理パス）を返す
- `RUN_CGI`: `getCgiContext()` が `script_filename`（物理パス）を返す

Session は、これらの物理パスに対して `stat/readdir/open` を実施し、「本当に存在するか」「ディレクトリか」「ファイルか」等を確認して最終応答を決めます。

---

## 7. `SERVE_AUTOINDEX` と `AutoIndexContext`

`SERVE_AUTOINDEX` は「ディレクトリ URI で、autoindex が有効で、index 候補が無い」ときに選ばれます。

`getAutoIndexContext()` は `SERVE_STATIC` と `SERVE_AUTOINDEX` のどちらでも呼べます。
返す `AutoIndexContext` は以下の情報を含みます。

- `directory_path`: ディレクトリの物理パス（root 配下として解決済み）
- `uri_dir_path`: 対象の URI（末尾 `/` を含む）
- `index_candidates`: index 候補の物理パス配列（`index` 設定から生成）
- `autoindex_enabled`: autoindex の ON/OFF

Session での典型フロー:

- `index_candidates` を順に `stat()` して存在するものがあればそれを返す
- どれも無い & `autoindex_enabled==true` ならディレクトリ一覧を生成して返す
- どれも無い & `autoindex_enabled==false` なら 404 等で応答

---

## 8. `RUN_CGI` と `CgiContext`

`RUN_CGI` は、location の `cgi_extensions` と request path のマッチで候補になります。
現行実装では、候補になった場合に **スクリプト実体を `stat()` で確認し**、不在/regular file でない場合は `404`（`RESPOND_ERROR`）になります。

`getCgiContext()` が返す `CgiContext`:

- `executor_path`: 実行ファイル（cgi_extension の値）
- `script_filename`: `root` 配下として解決したスクリプト物理パス
- `script_name`: URI 上の script 部分（拡張子終端まで）
- `path_info`: script_name 以降の URI（`/` を含み得る）
- `query_string`: リクエストのクエリ
- `http_minor_version`: HTTP/1.x の x

Session では `cgi_meta_variables.hpp` 等のルールに従って env を構築し、CGI プロセスの標準入出力をパイプで接続して実行します。

---

## 9. `STORE_BODY` と `UploadContext`

`STORE_BODY` は `upload_store` が設定され、メソッドが POST/PUT の場合に選ばれます。

`getUploadContext()` が返す `UploadContext`:

- `store_root`: 保存先ルート（upload_store）
- `target_uri_path`: 元の URI
- `destination_path`: `store_root` 配下として解決した保存先物理パス（末尾ファイルは未存在でも OK）
- `allow_create_leaf`: 現実装では `true`
- `allow_overwrite`: 現実装では `true`（同名ファイルがある場合は上書き）

Session 側は `allow_overwrite` を見て、既存ファイルがある場合の挙動（上書き拒否/許可）を決めます。

---

## 10. `clientMaxBodySize()` の使い方

`clientMaxBodySize()` は location の `client_max_body_size` を返します。

- Content-Length がある場合: LocationRouting 初期化時点で 413 判定済みになり得ます
- Content-Length が無い/不明（chunked など）の場合: Session が受信しながらこの値で上限を enforce します


# 責務分担

- LocationRouting（FS不要で決められる範囲）
  1. return（REDIRECT_* を確定）
  2. client_max_body_size（※Content-Length があれば事前に413。ただし chunked/不明は受信中に enforce）
  3. allow methods（405）
  + ルート解決に必要な材料（root、index候補、autoindex設定、cgi_extensions、upload_store、など）を Context として提供

- Session（FSが必要な範囲）

  4. 物理パス解決 + stat（存在/dir/file）
  5. dirなら index 探索→無ければ autoindex（or 403/404）
  6. fileなら拡張子で CGI or static（※この時点で `RUN_CGI` or `SERVE_STATIC` に確定）