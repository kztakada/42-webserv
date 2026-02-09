# Timeout / Session Handling

このドキュメントは、本プロジェクトにおける「セッションのタイムアウト」と「削除（破棄）タイミング」の最終仕様をまとめる。

対象コード（主に）:

- `server::Server::start()`
- `server::FdSessionController`
- `server::FdSession` および各 Session（`HttpSession`, `CgiSession`, `ListenerSession`）

## 目的

- I/O 多重化の待機（`waitEvents()`）に渡す timeout を **「次に発火すべきセッションタイムアウト」** に一致させる
- タイムアウトの対象外（listener など）が混ざっても、次回待機時間が歪まないようにする
- dispatch 中に delete して UAF にならないよう、削除の責務を Controller に集約する

## 用語と状態

- `timeout_seconds_`:
	- セッションのアイドルタイムアウト（秒）。
	- **`timeout_seconds_ <= 0` は「タイムアウト無し」** を意味する。
- `last_active_time_`:
	- `time(NULL)`（秒精度）で保持する最終アクティブ時刻。
	- 典型的には `handleEvent()` 冒頭で `updateLastActiveTime()` を呼ぶ。

### 「タイムアウト無し」の統一仕様

- `FdSessionController::getNextTimeoutMs()` は `getTimeoutSeconds() <= 0` のセッションを計算対象から除外する
- `FdSession::isTimedOut()` は `timeout_seconds_ <= 0` の場合、常に `false` を返す
- `ListenerSession` は `timeout_seconds_ = 0` を採用し、タイムアウト計測の対象外となる

この仕様により、listener を `active_sessions_` に delegate していても、最短タイムアウト計算に混入しない。

## イベントループにおけるタイムアウトの流れ

`Server::start()` の 1 ループは概ね次の順に進む。

1. `timeout_ms = FdSessionController::getNextTimeoutMs()`
2. `reactor_->waitEvents(timeout_ms)`
3. `FdSessionController::dispatchEvents(occurred_events)`
4. `FdSessionController::handleTimeouts()`

### `getNextTimeoutMs()` の返り値仕様

- `0`:
	- すでに期限切れのセッションが存在する（次の `handleTimeouts()` で即時処理したい）。
	- `waitEvents(0)` により「待機せず、すぐループを進める」。

実装上の補足（重要）:

	- `FdSessionController::getNextTimeoutMs()` は **無期限待機を使わない**。
		- タイムアウト対象のセッションが 1 つも無い場合でも `1000` を返す。
		- タイムアウト対象がある場合でも、返り値は最大 `1000ms` に clamp される。
		- 理由: `Server::start()` の `should_stop_` チェック直後にシグナルが届いた場合、`waitEvents()` が無期限にブロックすると shutdown できないため。

したがって `getNextTimeoutMs()` の返り値は次の通り:

- `0`:
	- 期限切れ（`remaining <= 0`）のセッションが存在する。
	- 次の `handleTimeouts()` で即時処理させたい。
- `1..1000`:
	- 最短の残り秒 `min_remaining_sec` を `min_remaining_sec * 1000` に変換し、`1000ms` を上限に clamp した値。
	- タイムアウト対象のセッションが 1 つも無い場合も `1000`。

計算の基準は次の通り:

- `elapsed = now - last_active_time_`（秒）
- `remaining = timeout_seconds_ - elapsed`（秒）

※ 時刻は `time(NULL)` のため **秒精度**。

### `handleTimeouts()` の仕様

- Controller は `active_sessions_` を走査し、`isTimedOut()` が `true` のセッションを収集する
- 収集したセッションに対して `kTimeoutEvent` を擬似的に生成し、`session->handleEvent(timeout_event)` を呼ぶ
	- `timeout_event.fd = -1`
	- `timeout_event.type = kTimeoutEvent`
- `handleEvent()` 呼び出し後、Controller は `requestDelete(session)` を実行して破棄を予約する

重要:

- タイムアウト時に **session 自身が自分を delete してはいけない**（Controller が delete する）
- session が `handleEvent(kTimeoutEvent)` 内で `controller_.requestDelete(this)` を呼んでも良い
	- `requestDelete()` は二重登録を避けるため、冪等に設計されている（同一 session の重複削除要求は無視される）

## 削除（破棄）と UAF 回避

- `FdSessionController::requestDelete(session)` は以下を行う:
	- `active_sessions_` から除外
	- watch を解除（紐づく fd を detach）
	- `deferred_delete_` に積み、**dispatch バッチ末尾で delete** する

`dispatchEvents()` は dispatch 中の delete による UAF を避けるため、イベント処理後にまとめて `deferred_delete_` を delete する。

加えて Controller は安全策として:

- dispatch 後に `session->isComplete()` が `true` のものを `requestDelete()` する
	- session 側が削除要求を出し忘れても回収できる

## セッション種別ごとのタイムアウト方針

### ListenerSession

- 目的: accept して `HttpSession` を生成し delegate する
- タイムアウト: 無し（`timeout_seconds_ = 0`）
- ねらい:
	- Listener は「接続を待ち続ける」のが役割であり、アイドルタイムアウトの概念を持たない
	- `getNextTimeoutMs()` の最短計算を歪めない

### HttpSession

- デフォルト: `kDefaultTimeoutSec = 10`
- `handleEvent()` 冒頭で `updateLastActiveTime()` を実施
	- read/write/error/timeout いずれのイベントでも「アクティブ」として扱う設計
	- 実際にタイムアウトになったセッションは `handleTimeouts()` の走査時点で確定しているため、`kTimeoutEvent` で時刻更新されても問題にならない
- `kTimeoutEvent` を受け取ったら、最小実装としてソケット shutdown し complete とする

例外（CGI関連）:

- CGI 実行中（`ExecuteCgiState`）は `HttpSession::isTimedOut()` が `false` を返し、HttpSession 自体のタイムアウトを抑制する
	- ねらい: HttpSession timeout で CloseWait に落ちて無言 close するより、CGI 側 timeout/error を起点に `504/502` を返す
- CGI headers は確定したが body が来るまでヘッダ送出を止めている間も同様に抑制する
	- 条件: `SendResponseState` かつ `pause_write_until_body_ready == true`
	- ねらい: 「ヘッダ後ハング」も CGI 側タイムアウトにより `504` へ差し替えられるようにする

### CgiSession

- デフォルト: `kDefaultTimeoutSec = 10`
- `handleEvent()` 冒頭で `updateLastActiveTime()` を実施
	- 追加仕様: 親がいる場合は `parent_session_->updateLastActiveTime()` も実施し、親 HttpSession のアイドル計測も同時に進める
- タイムアウト時は CGI プロセス/パイプをクリーンアップし、親（`HttpSession`）へ通知する設計
	- `kTimeoutEvent` / `kErrorEvent` を受け取ったら、親がいる場合 `HttpSession::onCgiError()` を 1 回だけ通知してから `controller_.requestDelete(this)` する
	- 通知メッセージは実装上 `"cgi session timeout"` / `"cgi session error"`

削除（破棄）に関する補足:

- 親（HttpSession）が存在する間は `CgiSession::isComplete()` が常に `false` になり、Controller の「dispatch後 self-complete 回収」では削除されない
	- ねらい: CGI stdout を HttpSession 側がストリーミングしている間に CgiSession が勝手に complete 扱いにならないようにする
	- 回収責務: 親がレスポンス送信完了時に `requestDelete(cgi_session)` する
	- ただし timeout/error の場合は CgiSession 側から `requestDelete` する

## Watch（fd監視）更新の注意

`FdSessionController` の watch 管理には次の重要な性質がある:

- `want_read == false` かつ `want_write == false` になった fd は、Controller が `unregisterFd(fd)` して **登録そのものが消える**
	- その後に `updateWatch(fd, ...)` を呼ぶと `"fd not registered"` になる
	- 監視を復帰させたい場合は `setWatch(fd, session, ...)` による再登録が必要

この仕様を踏まえ、HttpSession の socket watch 更新は:

- `updateWatch()` が失敗（未登録）した場合に `setWatch()` で再登録して復帰する
	- 例: CGI body 待ち等で一時的に read/write を両方止めた後、write を再開してレスポンス送信するケース

## 実装上の注意（拡張するとき）

- 「タイムアウト対象から除外したいセッション」は `timeout_seconds_ <= 0` を返す（または設定する）
	- 例: Listener / 長時間ストリーミング専用セッション / 無期限の管理セッション
- `updateLastActiveTime()` の呼びどころを変更する場合は、
	- 「何をアクティブとみなすか（read/write のみか、イベント全般か）」
	- 「no-op のイベント（たとえば内部通知）で更新すべきか」
	を先に仕様として固定する
- 秒精度（`time(NULL)`）のため、厳密な ms 精度のタイムアウトは保証しない
	- 必要になった場合は `gettimeofday` 等が候補だが、プロジェクトの使用可能関数制約と相談が必要
