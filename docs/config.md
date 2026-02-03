# config/ の読み方（初心者向け）

このドキュメントは、`srcs/server/config` ディレクトリの実装を読んで「設定ファイルをどうパースして、どんなオブジェクトに詰めて、どこで使うか」をまとめたものです。

- 設定ファイルの仕様（ディレクティブ一覧や例）はまず [docs/conf_spec.md](conf_spec.md) を参照してください。
- ここでは **実装上のクラス構成（階層）** と **データの流れ（使い方）** にフォーカスします。

---

## 1. 全体像（設定 → オブジェクト化 → サーバー起動）

1. `main.cpp` が設定ファイルを読み込む
2. `ConfigParser` が `ServerConfig` を作る
3. `Server::create(config)` に渡して起動

実際の入口は以下です。

- パース: `server::ConfigParser::parseFile()`
- サーバー起動: `server::Server::create(config)`

ざっくりのデータフロー（階層構造）はこうなっています。

```
設定ファイル
  └─ server { ... } が複数
       └─ location { ... } が複数

C++オブジェクト
  ServerConfig
    └─ servers: vector<VirtualServerConf>
         ├─ listens / server_names / root_dir / index_pages / ...
         └─ locations: vector<LocationDirectiveConf>
              ├─ path_pattern / allowed_methods / cgi_extensions / ...
              └─ (必要なら) return / upload_store / autoindex / ...
```

---

## 2. config ディレクトリのファイル構成

### 2.1 ルート直下（設定データ本体）

- `server_config.hpp/.cpp`
  - `ServerConfig`: 設定全体（serverブロックの配列）
- `virtual_server_conf.hpp/.cpp`
  - `VirtualServerConf`: serverブロック相当（バーチャルホスト単位）
  - `Listen`: `listen` の1要素（IP + port）
- `location_directive_conf.hpp/.cpp`
  - `LocationDirectiveConf`: locationブロック相当（パス単位のルール）

### 2.2 parser/（設定ファイル文字列 → Conf構築）

- `parser/config_parser.hpp/.cpp`
  - `ConfigParser`: 字句読み取り + 文法解釈 + Confへの格納
  - `ConfigParser::ParseContext`: 低レベルのトークンリーダー
- `parser/conf_maker.hpp`
  - `VirtualServerConfMaker` / `LocationDirectiveConfMaker`
  - 直接Confをいじらず **「パースしながら安全に組み立てる」** ための薄いビルダー

---

## 3. クラス構成（役割・責務）

### 3.1 `ServerConfig`（最上位）

- 役割: 設定全体をまとめるコンテナ
- 中身: `servers: vector<VirtualServerConf>`
- 重要メソッド:
  - `appendServer()`
    - 追加前に `VirtualServerConf::isValid()` をチェック
  - `isValid()`
    - serverが1つ以上あること、各serverがvalidであること

ポイント: **「ServerConfigがvalidでない設定は起動できない」** という最終ゲートになっています。

### 3.2 `VirtualServerConf`（serverブロック）

- 役割: `server { ... }` ブロック1つ分
- 主なフィールド:
  - `listens: vector<Listen>`: `listen` の集合
  - `server_names: set<string>`: `server_name` の集合
  - `root_dir: FilePath`（物理パス）
  - `index_pages: vector<FileName>`
  - `client_max_body_size`（デフォルト 1MB）
  - `error_pages: ErrorPagesMap`
  - `locations: vector<LocationDirectiveConf>`

- 重要メソッド（パーサーから呼ばれる「登録API」）:
  - `appendListen()` / `appendServerName()`
  - `setRootDir()`
  - `appendIndexPage()`
  - `setClientMaxBodySize()`
  - `appendErrorPage()`
  - `appendLocation()`

- `getListens()`
  - 起動時に `bind()` すべきlisten一覧を返すためのユーティリティ
  - **重複排除** と **ワイルドカード(0.0.0.0)優先** を解決します

- `isValid()`
  - 必須: `listen` が1つ以上、`root_dir` がセット済み
  - `client_max_body_size` が範囲内
  - locationが全てvalid

### 3.3 `LocationDirectiveConf`（locationブロック）

- 役割: `location <pattern> { ... }` ブロック1つ分
- 主なフィールド:
  - `path_pattern: URIPath`（`/`開始必須）
  - `allowed_methods: set<HttpMethod>` と `has_allowed_methods`
  - `root_dir` と `has_root_dir`
  - `index_pages` と `has_index_pages`
  - `client_max_body_size` と `has_client_max_body_size`
  - `cgi_extensions: map<.ext, FilePath>`
  - `error_pages` と `has_error_pages`
  - `auto_index` と `has_auto_index`
  - `redirect_status` / `redirect_url`（`return` 用）
  - `upload_store`（保存先ディレクトリ）
  - `is_backward_search`（`location back ...` 用）

- 重要メソッド:
  - `setPathPattern()`
  - `appendAllowedMethod()`（GET/POST/DELETEのみ）
  - `setRootDir()` / `setUploadStore()`（物理パスとして解決）
  - `appendCgiExtension()`（`.ext` → 実行ファイルパス）
  - `setAutoIndex()`（on/off）
  - `setRedirect()`（3xx code + URL/URI）

- `isValid()`
  - returnが「codeとurlの両方が揃う」ことを保証（片方だけはinvalid）
  - `client_max_body_size` 範囲
  - `cgi_extensions` のキー/値が空でない など

---

## 4. 型（value_types の意味）

設定値は、意味が分かりやすいように `typedef` されています。

- `FileName`: `std::string`
- `URIPath`: `/` で始まるURIパス
- `TargetURI`: URIまたはURL（`/...` または `http(s)://...`）
- `CgiExt`: `.py` のような拡張子文字列
- `FilePath`: `utils::path::PhysicalPath`（正規化済み絶対パス）
- `ErrorPagesMap`: `map<HttpStatus, TargetURI>`

物理パスの解決は `utils::path::PhysicalPath::resolve()` に寄せています。

- `root`, `upload_store`, `cgi_extension` の実行ファイルパスは **起動時に絶対パスへ解決・正規化して保持**
- `return`, `error_page` のようなURI/URLは **文字列として保持**（物理パス解決しない）

この方針は [docs/conf_spec.md](conf_spec.md) の「path / url の記述・パース仕様」に対応しています。

---

## 5. `ConfigParser` の仕組み（読み進め方）

### 5.1 低レイヤ: `ParseContext` がトークンを読む

`ParseContext` は「文字列を1文字ずつ読む」ための薄いユーティリティです。

- `skipSpaces()`
  - 空白を飛ばす
  - `#` から行末までコメントとしてスキップ
- `getWord()`
  - 次のトークンを返す
  - `{`, `}`, `;` は単独トークンとして返す

つまり、パーサーは

- `getWord()` でトークン列を得る
- それが `listen` なら listen処理、`location` なら location処理…

という「手書きパーサー」になっています。

### 5.2 上位: serverブロックとlocationブロックを組み立てる

- `parseConfigInternal()`
  - ファイル全体を走査
  - `server` しかトップレベルに置けない

- `parseServerBlock()`
  - `server { ... }` を処理
  - `VirtualServerConfMaker` に値を詰める
  - 終端 `}` で `build()` して `ServerConfig` に追加

- `parseLocationBlock()`
  - `location <path> { ... }` を処理
  - `LocationDirectiveConfMaker` に値を詰める
  - location内の重複ディレクティブを一部チェック（後述）

### 5.3 `location back`（後方一致）の扱い

`parseServerBlock()` で `location` の直後に `back` が来た場合、

- `LocationDirectiveConf::is_backward_search = true` にする

というフラグとして保持します。

実際に「prefix一致かsuffix一致か」を使ってルーティングするのは、configディレクトリではなく routing 側（location選択ロジック）です。

---

## 6. ディレクティブ → どのConfに入る？（対応表）

### 6.1 serverコンテキスト

| ディレクティブ | 格納先 | 備考 |
|---|---|---|
| `listen` | `VirtualServerConf::listens` | `<port>` または `<ip>:<port>`。同一server内の完全重複は禁止 |
| `server_name` | `VirtualServerConf::server_names` | 同一portで同一server_nameが複数serverに出るのはエラー（ただし空文字は複数OK） |
| `root` | `VirtualServerConf::root_dir` | `PhysicalPath::resolve()` で解決（重複指定はエラー） |
| `index` | `VirtualServerConf::index_pages` | 1つ以上必要（無引数はエラー） |
| `client_max_body_size` | `VirtualServerConf::client_max_body_size` | `10M` のようにM指定可。重複指定はエラー |
| `error_page` | `VirtualServerConf::error_pages` | `code` は4xx/5xxのみ許可（実装でチェック） |
| `location` | `VirtualServerConf::locations` | `LocationDirectiveConf` を追加 |

### 6.2 locationコンテキスト

| ディレクティブ | 格納先 | 備考 |
|---|---|---|
| `allow_methods` | `LocationDirectiveConf::allowed_methods` | 1つ以上必要（無引数はエラー） |
| `cgi_extension` | `LocationDirectiveConf::cgi_extensions` | `.ext` と実行ファイルパス（物理パス解決） |
| `return` | `redirect_status` + `redirect_url` | 3xxのみ許可。`code url` 両方必須 |
| `root` | `LocationDirectiveConf::root_dir` | 物理パス解決。location内で重複指定はエラー |
| `index` | `LocationDirectiveConf::index_pages` | 1つ以上必要（無引数はエラー） |
| `client_max_body_size` | `LocationDirectiveConf::client_max_body_size` | `M`対応。location内重複はエラー |
| `error_page` | `LocationDirectiveConf::error_pages` | 4xx/5xxのみ |
| `autoindex` | `LocationDirectiveConf::auto_index` | `on/off` 以外はエラー。location内重複はエラー |
| `upload_store` | `LocationDirectiveConf::upload_store` | 物理パス解決。location内重複はエラー |

---

## 7. バリデーション（よくあるつまずきポイント）

### 7.1 必須項目

- serverブロックは
  - `listen` が最低1つ
  - `root` が必須

これが満たされないと `VirtualServerConf::isValid()` が false になり、起動前に弾かれます。

### 7.2 location内の「一部ディレクティブだけ」重複検出

`ConfigParser` は location内の重複を、以下のディレクティブに限って検出しています。

- `client_max_body_size`
- `root`
- `autoindex`
- `upload_store`
- `return`

（`index` や `error_page` などは複数書ける前提の設計です）

### 7.3 `return` の整合性

- `return` は **codeとurlがセットで存在** することを `LocationDirectiveConf::isValid()` が強制します。

---

## 8. 使い方（最短で動かす）

設定ファイルを用意して、実行時引数に渡します。

- `./webserv <config_file>`

内部では [srcs/main.cpp](../srcs/main.cpp) が

- `ConfigParser::parseFile(argv[1])`
- `Server::create(config)`

の順で呼びます。

---

## 9. 次に読むと理解がつながる場所

- 設定仕様: [docs/conf_spec.md](conf_spec.md)
- locationのマッチング（prefix/suffix/最長一致）: docs内の routing 関連ドキュメント（例: `location_routing.md`）
- 設定の利用側（Hostヘッダでのバーチャルホスト選択など）: `server/request_router` 周辺
