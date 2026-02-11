## プロジェクト名
webserv - C++98で書かれたシンプルなHTTPサーバー

## プロジェクトの概要
以下の要件で、httpサーバーを設計しています。
- C++98 でイベント駆動、非同期IOを使ってHTTPサーバーを書く｡
	- ただし、Nginxのように1つのMasterプロセスと複数のWorkerプロセスで稼働するマルチプロセスの構成ではなく、Workerプロセスが1つだけあるシングルプロセスシングルスレッドの構成。
- OSのソケットインターフェースを用いてクライアントと通信する｡
	- ソケットより下のレイヤーはOSにおまかせする。
- NginxのようにノンブロッキングI/OとI/O多重化を用いてリクエストを処理する｡ApacheのようにリクエストごとにForkしない｡
- CGIに対応させる｡
- NginxではFastCGIだが、このプロジェクトでは別プロセスで非同期実行させるCGIを実装する。
    - CGIはパイプで標準入出力を接続し、HTTPリクエストボディを標準入力に渡し、標準出力からCGIレスポンスを受け取る｡
- プログラム実行時に引数として指定された設定ファイルを読み込み、サーバーの挙動を制御する｡
- HTTPリクエストメソッドについて、GET/POST/DELETEのみを実装する。 GET/POST/DELETE以外は 501エラー(NOT IMPLEMENTED)とする

## 設計指針
### 全体方針
- オブジェクト指向に基づく設計を採用し、責務や機能をクラスとして明確に分割する｡
- シンプルで理解しやすい設計を心がける｡
- 拡張性を考慮し、将来的な機能追加や変更に対応しやすい設計を目指す｡
- 各コンポーネントは単一責任の原則に従い、1つの責任に集中させる｡
- 依存性の注入を活用し、コンポーネント間の結合度を低く保つ｡
- ネームスペースの階層は減らして、フォルダ分けで管理しやすくする方針。

### 禁止事項
 - 標準関数は以下の関数以外使用禁止
  - `execve()`, `pipe()`, `strerror()`, `gai_strerror()`, `errno`, `dup()`,
`dup2()`, `fork()`, `socketpair()`, `htons()`, `htonl()`, `ntohs()`, `ntohl()`,
`select()`, `poll()`, `epoll()`, `epoll_create()`, `epoll_ctl()`,
`epoll_wait()`, `kqueue()`, `kevent()`, `socket()`,
`accept()`, `listen()`, `send()`, `recv()`, `chdir()`, `bind()`, `connect()`,
`getaddrinfo()`, `freeaddrinfo()`, `setsockopt()`, `getsockname()`,
`getprotobyname()`, `fcntl()`, `close()`, `read()`, `write()`, `waitpid()`,
`kill()`, `signal()`, `access()`, `stat()`, `open()`, `opendir()`, `readdir()`, `closedir()`
- STLライブラリは使用可能
- その他の外部ライブラリとBoostは使用禁止

## コーディングStyleガイドライン

### 1. 言語仕様とコンパイラ
- **C++98 準拠**: C++11以降の機能は使用しない
- **コンパイラオプション**: `-I srcs` でインクルードパスを指定

### 2. 命名規則
- **ファイル名**: 小文字とアンダースコア（例: `http_request.hpp`, `cgi_executor.cpp`）
- **クラス名**: パスカルケース（例: `HttpRequest`, `Server`, `CgiExecutor`）
- **関数名**: キャメルケース（例: `handleRequest()`, `createErrorResponse()`）
- **変数名**: スネークケース（例: `event_manager_`, `should_stop_`, `cgi_output_fd`）
- **メンバ変数**: 末尾にアンダースコア（例: `event_manager_`, `config_`, `phase_`）
- **定数**: `k` プレフィックス + パスカルケース（例: `kCgiTimeoutSeconds`, `kRequestLine`）
- **列挙型**: パスカルケース、値は大文字スネークケース（例: `ParsingPhase`, `OK`, `ERROR`, `BAD_REQUEST`）
- **名前空間**: 小文字（例: `server`, `http`, `utils`）

### 3. 名前空間構成
```cpp
namespace server           // プロジェクトルート名前空間
namespace http              // HTTP プロトコル関連
namespace utils             // ユーティリティ
  namespace result          // Result型の実装
```

### 4. ヘッダーガード
- `#ifndef` / `#define` / `#endif` 形式を使用
- 形式: `<NAMESPACE>_<FILENAME>_HPP_`
- 例: `#ifndef WEBSERV_SERVER_HPP_`, `#ifndef HTTP_HTTP_REQUEST_HPP_`

### 5. インクルード順序
1. 対応するヘッダーファイル（.cpp ファイルの場合）
2. 標準ライブラリ
3. システムヘッダー
4. プロジェクト内ヘッダー（相対パスで指定）

```cpp
#include "server/server.hpp"  // 対応するヘッダー
#include <iostream>            // 標準ライブラリ
#include <csignal>             // システムヘッダー
#include "http/http_request.hpp"  // プロジェクト内
```

### 6. クラス設計
- **前方宣言**: 必要に応じてコンパイル時間を短縮
- **Factory パターン**: 複雑な初期化が必要な場合は static な `create()` メソッドを提供
- **コピー禁止**: コピーコンストラクタと代入演算子を private に配置（リソース管理クラス）
- **デストラクタ**: 仮想関数を持つ基底クラスには virtual デストラクタを実装
- **メンバ変数の順序**: public → protected → private
- **メソッドの順序**: public → private

```cpp
class Server {
public:
    static Result<Server*> create(const config::Config& config);  // Factory
    ~Server();
    void start();
    
private:
    explicit Server(const config::Config& config);  // private コンストラクタ
    Server(const Server&);                          // コピー禁止
    Server& operator=(const Server&);               // 代入禁止
};
```

### 7. エラーハンドリング
- **Result 型を使用**: Rust の Result 型を模倣した独自実装を使用
- **成功時**: 値を直接返す（暗黙の型変換）
- **失敗時**: `Result<T>(ERROR, "error message")` を返す
- **void の場合**: `Result<void>()` を返す

```cpp
Result<int> doSomething() {
    if (success) {
        return 42;  // 暗黙の型変換
    }
    return Result<int>(ERROR, "Failed to do something");
}

// 呼び出し側
Result<int> result = doSomething();
if (result.isOk()) {
    int value = result.unwrap();
}
```

### 8. コメント
- **ヘッダーファイル**: クラスや関数の役割を簡潔に記述
- **実装ファイル**: 複雑なロジックに対して説明を追加
- **TODO/NOTE**: 必要に応じて使用
- **RFC 準拠**: HTTP 関連は RFC 番号を明記（例: `// RFC 7230 Section 3.2`）

### 9. インデントとフォーマット
- **インデント**: タブを使用（スペース幅は環境により異なる）
- **ブレース**: 関数定義・制御構文は改行してから開く（K&R スタイル変形）
- **名前空間**: ブレース後にコメントでクローズ（例: `} // namespace webserv`）

```cpp
void Server::start()
{
    while (!should_stop_)
    {
        // process events
    }
}
```

### 10. 型とキャスト
- **型推論なし**: C++98 のため `auto` は使用不可
- **キャスト**: C++ スタイルキャストを優先（`static_cast`, `reinterpret_cast` など）
- **ポインタと参照**: 
  - 所有権を持たない参照渡しは `const &`
  - 変更する場合は非 const 参照またはポインタ

### 11. STL の使用
- **コンテナ**: `std::vector`, `std::map`, `std::string` を積極的に使用
- **イテレータ**: 明示的な型宣言が必要（C++98）
```cpp
for (std::vector<FdEvent>::const_iterator it = events.begin();
     it != events.end(); ++it) {
    dispatchEvent(*it);
}
```

### 12. メモリ管理
- **生ポインタ**: C++98 のため `new` / `delete` を使用
- **責任の明確化**: 誰が delete するかを明確にする
- **所有権**: Factory で作成したオブジェクトは呼び出し側が delete
- **デストラクタでクリーンアップ**: 必ず delete を実行

```cpp
Server* server = server_result.unwrap();
// ... 使用 ...
delete server;  // 呼び出し側で delete
```

### 13. シグナルハンドリング
- **volatile sig_atomic_t**: シグナルハンドラで変更するフラグに使用
- **static メンバ**: シグナルハンドラから参照する変数

```cpp
volatile sig_atomic_t should_stop_;
static Server* running_instance_;
static void signalHandler(int signum);
```

### 14. ログとエラー出力
- **標準エラー出力**: `std::cerr` を使用
- **標準出力**: `std::cout` を使用（通常のログ）

### 15. 設計原則
- **単一責任の原則**: 各クラスは1つの責任を持つ
- **依存性の注入**: コンストラクタで依存オブジェクトを受け取る
- **イベント駆動**: ノンブロッキングI/Oとイベントループを基本とする
- **非同期実行**: CGI は別プロセスで実行し、パイプで通信
- **OS 抽象化**: OS 固有の実装は `FdEventReactor` として抽象化実装して分離（kqueue, epoll, select）

### 16. ファイル構成
```
srcs/
  main.cpp              - エントリーポイント
  http/                 - HTTP プロトコル層
  network/              - 共通ネットワーク関連（ソケット操作など）
  utils/                - 汎用ユーティリティ（Result型など）
  server/              - サーバー実装
    config/              - サーバー起動設定ファイル関連
    reactor/            - イベントリアクタ実装（FdEventライフサイクル管理）
    request_router/     - リクエストルーティング（仮想サーバー、ロケーション）
    session/            - サーバー内セッション管理
        fd/              - ファイルディスクリプタ関連
            tcp_socket/    - TCPソケットクラス実装
            cgi_pipe/     - CGIパイプクラス実装
        fd_session/     - fdセッションクラス実装（リスナー、http、cgiセッション）
        fd_session_controller.cpp - fdセッションコントローラー実装（セッションライフサイクル管理）
    server.cpp          - サーバークラス実装
Makefile                - ビルド設定
```
