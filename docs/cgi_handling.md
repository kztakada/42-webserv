# CGI Handling（timeout / error / status）

このドキュメントは、本プロジェクトの CGI 実行に関する「HTTP ステータスの決め方」と「タイムアウト/エラー時の扱い」を、現行実装に合わせてまとめる。

対象コード（主に）:

- `server::LocationRouting::decideAction_()`
- `server::SessionCgiHandler`
- `server::CgiSession`
- `server::HttpSession`（特に `isTimedOut()` と `SendResponseState`）

## 1. 404（CGIスクリプト不在）はルーティング段階で確定する

CGI 拡張子がマッチした場合でも、実装は `LocationRouting::decideAction_()` 内でスクリプトの実体を `stat()` し、
以下のときは **CGI実行エラーではなく「リソース未存在」**として `404 Not Found` を選ぶ。

- `stat()` が失敗（存在しない等）
- regular file ではない（`S_ISREG` でない）

この場合:

- `LocationRouting.getHttpStatus()` は `404`
- `LocationRouting.getNextAction()` は `RESPOND_ERROR`（error_page があれば内部リダイレクトに切り替わり得る）

目的:

- CGI の spawn 失敗や実行時異常（502）と、スクリプト不在（404）を明確に区別する

## 2. 502 / 504 は Session 層で CGI エラーを受けて決める

CGI 実行は `SessionCgiHandler::startCgi()` が担当し、`CgiSession` を delegate して pipe を監視する。

CGI の失敗は `HttpSession::onCgiError()` 経由で `SessionCgiHandler::handleCgiError_()` に集約され、
ここで `502 Bad Gateway` または `504 Gateway Timeout` が選ばれる。

### 2.1 504（Gateway Timeout）になる条件

`SessionCgiHandler::handleCgiError_()` は、エラーメッセージ文字列に `"timeout"` を含む場合に `504` を返す。

現行実装で `timeout` を含む代表ケース:

- `CgiSession` が `kTimeoutEvent` を受けたとき
	- 親へ `"cgi session timeout"` を通知する

この `kTimeoutEvent` は `FdSessionController::handleTimeouts()` が `CgiSession::isTimedOut()` を見て発火させる。

### 2.2 502（Bad Gateway）になる条件

`timeout` 以外の CGI 由来エラーは `502` を返す。

例:

- CGI の stdout が headers 完了前に閉じた（`"cgi stdout closed before headers"` 等）
- headers パース/適用に失敗した
- その他 `CgiSession` 内部処理の失敗（I/O 不能等）

## 3. 「headers後ハング」でも 504 に落とすための設計

課題:

- CGI が headers を出した後に body を出さずにハングすると、headers をクライアントへ送ってしまった後では HTTP ステータスを 504 に差し替えられない。

現行実装の戦略:

- `CgiSession` は stdout から headers をパースし、headers 完了時点で親へ通知する
- 親（`HttpSession`）は **最初の body 1 byte が到着するまでヘッダ送出を遅延**する
	- stdout の残り（ヘッダ終端と同じ read で先読みした分）は `prefetched_body` として保持
	- stdout fd は `HttpSession` 側へ移譲し、`SendResponseState` が body fd の read を監視する

この結果:

- headers 完了後に body が来ないまま `CgiSession` が timeout した場合でも、まだ headers 未送出なので `504` に差し替えて応答できる

補足:

- すでに `prefetched_body` がある場合（headers と同時に body の先頭が来た場合）は、その時点で送信を開始する

## 4. タイムアウト責務の分担（HttpSession vs CgiSession）

- CGI 実行中は `HttpSession` 自体のタイムアウト判定を抑制する（`HttpSession::isTimedOut()==false`）
	- 対象: `ExecuteCgiState` / `SendResponseState` + `pause_write_until_body_ready`（body 待ち中）
	- ねらい: HttpSession の timeout で無言 close せず、CGI timeout を起点に 504 を返す

- CGI のタイムアウトは `CgiSession` が担当し、timeout/error を親へ通知して 504/502 を組み立てる

## 5. 後始末（zombie/リーク防止）

- レスポンス送信完了時、`HttpSession` は紐づく `active_cgi_session` を `requestDelete()` して確実に回収する
- `CgiSession` の destructor は watch 解除と `waitpid/kill` を行い、子プロセスを回収する

関連事項:

- watch を全 off にすると Controller が fd を unregister する挙動があるため、監視復帰時は再登録が必要になる（詳細は `docs/timeout_session_handring.md` を参照）
