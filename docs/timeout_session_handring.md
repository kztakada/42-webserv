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

- `-1`:
	- タイムアウト対象のセッションが 1 つも無い（`timeout_seconds_ <= 0` のみ、または空）。
	- `waitEvents(-1)` は無期限待機の意味（Reactor 実装側の仕様に従う）。
- `0`:
	- すでに期限切れのセッションが存在する（次の `handleTimeouts()` で即時処理したい）。
	- `waitEvents(0)` により「待機せず、すぐループを進める」。
- `> 0`:
	- 最短の残り秒 `min_remaining_sec` を `min_remaining_sec * 1000` に変換した ms。
	- `int` への変換に際し、上限を `INT_MAX` 相当に clamp する。

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

- デフォルト: `kDefaultTimeoutSec = 5`
- `handleEvent()` 冒頭で `updateLastActiveTime()` を実施
	- read/write/error/timeout いずれのイベントでも「アクティブ」として扱う設計
	- 実際にタイムアウトになったセッションは `handleTimeouts()` の走査時点で確定しているため、`kTimeoutEvent` で時刻更新されても問題にならない
- `kTimeoutEvent` を受け取ったら、最小実装としてソケット shutdown し complete とする

### CgiSession

- デフォルト: `kDefaultTimeoutSec = 5`
- `handleEvent()` 冒頭で `updateLastActiveTime()` を実施
- タイムアウト時は CGI プロセス/パイプをクリーンアップし、親（`HttpSession`）へ通知する設計

## 実装上の注意（拡張するとき）

- 「タイムアウト対象から除外したいセッション」は `timeout_seconds_ <= 0` を返す（または設定する）
	- 例: Listener / 長時間ストリーミング専用セッション / 無期限の管理セッション
- `updateLastActiveTime()` の呼びどころを変更する場合は、
	- 「何をアクティブとみなすか（read/write のみか、イベント全般か）」
	- 「no-op のイベント（たとえば内部通知）で更新すべきか」
	を先に仕様として固定する
- 秒精度（`time(NULL)`）のため、厳密な ms 精度のタイムアウトは保証しない
	- 必要になった場合は `gettimeofday` 等が候補だが、プロジェクトの使用可能関数制約と相談が必要
