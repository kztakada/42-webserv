#include "server/server.hpp"

#include <unistd.h>

#include <csignal>
#include <deque>

#include "utils/log.hpp"

namespace server
{
using namespace utils;
using namespace utils::result;

Server* Server::running_instance_ = nullptr;

Server::Server(const ServerConfig& config)
    : event_manager_(FdEventReactorFactory::create()),
      router_(new RequestRouter(config)),
      should_stop_(false),
{
    request_handler_ = new RequestHandler(cgi_executor_, router_, config);
    if (running_instance_)
        throw std::runtime_error("Only one Server instance can run at a time.");
    running_instance_ = this;
}

Server::~Server()
{
    delete fd_event_reactor_;
    delete fd_session_controller_;
    delete router_;
    if (running_instance_ == this)
        running_instance_ = nullptr;
    stop();  // デストラクタで確実に停止
}

void Server::start()
{
    should_stop_ = false;
    signal(SIGINT, signalHandler);   // Ctrl+C
    signal(SIGTERM, signalHandler);  // kill
    signal(SIGPIPE, SIG_IGN);        // SIGPIPE無視（write時のエラーで処理）

    while (!should_stop_)
    {
        // 1. 次のタイムアウト時間を計算
        int timeout_ms = timeout_manager_->getNextTimeoutMs();
        // 2. イベント待機
        Result<const std::vector<FdEvent>&> events_result =
            reactor_->waitEvents(timeout_ms);
        if (events_result.isError())
        {
            Log::error("Event wait failed: ", events_result.getErrorMessage());
            continue;
        }
        // イベント取得成功
        const std::vector<FdEvent>& occurred_events = events_result.unwrap();

        // 3. イベント処理
        session_controller_->dispatchEvents(occurred_events);

        // 4. タイムアウト処理
        session_controller_->handleTimeouts();
    }

    session_controller_->clearAllSessions();
    reactor_->clearAllEvents();
    Log::info("Server stopped.");
}

void Server::stop()
{
    if (running_instance_ == this)
        running_instance_ = nullptr;
    should_stop_ = true;  // 停止フラグを立てる
}

//------------------------------------------------------------
// private

Result<void> Server::initialize()
{
    // 1. リスナーソケットの作成
    Result<void> listener_result = listener_manager_->initialize(config_);
    if (listener_result.isError())
    {
        return listener_result;  // エラーをそのまま返す
    }

    // 2. 各リスナーをFdEventReactorに登録
    std::vector<FdSession> sessions = listener_manager_->getListenerSessions();

    for (std::vector<FdSession>::iterator it = sessions.begin();
        it != sessions.end(); ++it)
    {
        Result<void> add_result = event_manager_->add(&(*it));
        if (add_result.isError())
        {
            return add_result;  // エラーをそのまま返す
        }
    }

    Log::info("Server initialized with ", sessions.size(), " listeners");
    return Result<void>();  // 成功
}

void Server::dispatchEvent(const FdEvent& event)
{
    // FdSessionのコールバックを呼び出すか、
    // または型に応じてハンドラーを分岐
    if (listener_manager_->hasListener(event.session->fd))
    {
        handleListenerEvent(event.session->fd, event.triggered_event);
    }
    else if (connection_manager_->hasConnection(event.session->fd))
    {
        handleConnectionEvent(event.session->fd, event.triggered_event);
    }
    else
    {
        // CGI FDの存在チェック
        if (cgi_executor_->hasCgiSession(event.session->fd))
        {
            // CGI出力FD（読み込み）
            handleCgiOutputEvent(event.session->fd, event.triggered_event);
        }
        else
        {
            // CGI入力FDの可能性（書き込み）
            handleCgiInputEvent(event.session->fd, event.triggered_event);
        }
    }
}

void Server::handleListenerEvent(int fd, FdEventType events)
{
    if (!(events & kReadEvent))
        return;

    ListenSocket* listen_sock = listener_manager_->getListener(fd);

    // Accept実行
    Result<std::pair<ConnSocket*, FdSession*>> result =
        listener_manager_->acceptConnection(listen_sock);

    if (result.isError())
    {
        // エラーログ
        Log::error("Accept failed: ", result.getErrorMessage().c_str());
        return;
    }

    ConnSocket* conn = result.unwrap().first;
    FdSession* conn_session = result.unwrap().second;

    Log::info("New connection: fd=", conn->GetFd(), " from ",
        conn->GetRemoteIp() + ":" + conn->GetRemoteName());

    // ConnectionManagerに追加
    connection_manager_->addConnection(conn);

    // タイムアウトを登録
    timeout_manager_->registerTimeout(
        conn->GetFd(), ConnSocket::kDefaultTimeoutMs);

    // FdEventReactorに登録
    Result<void> add_result = event_manager_->add(conn_session);
    if (add_result.isError())
    {
        Log::error("Failed to add connection to event manager: ",
            add_result.getErrorMessage());
        closeConnection(conn->GetFd());
    }
}

void Server::handleConnectionEvent(int fd, FdEventType events)
{
    ConnSocket* conn = connection_manager_->getConnection(fd);
    if (!conn)
        return;

    // タイムアウトを更新（アクティビティがあった）
    timeout_manager_->updateTimeout(fd);

    bool should_close = false;

    // エラーイベント
    if (events & kErrorEvent)
    {
        Log::warning("Error event on fd=", fd);
        should_close = true;
    }

    // 読み込みイベント
    if (!should_close && (events & kReadEvent))
    {
        should_close = handleRead(conn);
    }

    // 書き込みイベント
    if (!should_close && (events & kWriteEvent))
    {
        should_close = handleWrite(conn);
    }

    if (should_close)
    {
        closeConnection(fd);
    }
}

void Server::handleTimeout(int fd)
{
    // タイムアウトなのでログを出して切断
    Log::info("Connection timeout: fd=", fd);
    closeConnection(fd);
}

bool Server::handleRead(ConnSocket* conn)
{
    // 1. 【1回だけread】ノンブロッキングなので可能な分だけ読む
    char buf[8192];
    ssize_t n = read(conn->GetFd(), buf, sizeof(buf));

    if (n < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            // データがもうない（これは正常）
            return false;  // 次のイベントを待つ
        }
        Log::error("Read error on fd=", conn->GetFd(), ": ", strerror(errno));
        return true;  // 接続を閉じる
    }

    if (n == 0)
    {
        Log::info("Client closed connection: fd=", conn->GetFd());
        return true;  // クライアントが切断
    }

    Log::debug("Read ", n, " bytes from fd=", conn->GetFd());
    // 2. バッファに追加（まだ不完全かもしれない）
    // utils::ByteVector &buffer = conn->GetBuffer();
    // buffer.append(reinterpret_cast<const unsigned char *>(buf),
    // 			  static_cast<size_t>(n));
    conn->GetBuffer().append(
        reinterpret_cast<const unsigned char*>(buf), static_cast<size_t>(n));

    // 3. HTTPリクエストをパース
    std::deque<http::HttpRequest>& requests = conn->GetRequests();
    http::HttpRequest request;
    Result<void> parse_result = request.parse(conn->GetBuffer());

    if (parse_result.isOk())
    {
        // リクエストが完成した場合、キューに追加
        if (request.isParseComplete())  // Completeしなかった場合どうするの？
        {
            http::HttpResponse* response =
                request_handler_->handleRequest(request, conn);

            // レスポンスにNULLが返ってきた場合はCGI処理中
            if (response == NULL)
            {
                // ★ CGI処理中
                CgiSession* cgi_session = conn->GetCgiSession();

                if (cgi_session)
                {
                    Log::info("CGI processing: client_fd=", conn->GetFd(),
                        ", cgi_fd=", cgi_session->cgi_output_fd);
                    // CGI出力FDをepollに登録
                    FdSession output_session = FdSession::create(
                        cgi_session->cgi_output_fd, kReadEvent, nullptr);
                    event_manager_->add(output_session);

                    // リクエストボディがあればCGI入力FDも登録（書き込みイベント）
                    if (!cgi_session->input_complete &&
                        cgi_session->cgi_input_fd >= 0)
                    {
                        FdSession input_session = FdSession::create(
                            cgi_session->cgi_input_fd, kWriteEvent, NULL);
                        event_manager_->add(input_session);
                    }

                    // クライアントFDは一旦監視解除
                    event_manager_->remove(conn->GetFd());
                }

                return false;
            }

            conn->SetResponse(response);
        }
    }
    else
    {
        // パースエラー
        Log::error("Parse error on fd=", conn->GetFd(), ": ",
            parse_result.getErrorMessage());
        // 400 Bad Requestを返す
        http::HttpResponse* error_response = new http::HttpResponse(400, conn);
        conn->SetResponse(error_response);
    }

    // 書き込み監視に切り替え
    FdSession session = FdSession::create(conn->GetFd(), kWriteEvent, nullptr);
    event_manager_->setWatchedEvents(session);

    return false;  // 接続継続
}

void Server::handleCgiOutputEvent(int cgi_fd, FdEventType events)
{
    CgiSession* cgi_session = cgi_executor_->getSessionByFd(cgi_fd);

    if (!cgi_session)
    {
        Log::error("Unknown CGI fd: ", cgi_fd);
        return;
    }

    // CGI出力を読み込み
    Result<bool> read_result = cgi_executor_->readCgiOutput(cgi_session);

    if (read_result.isError())
    {
        Log::error("CGI read error: ", read_result.getErrorMessage());

        // エラーレスポンスを作成
        ConnSocket* conn =
            connection_manager_->getConnection(cgi_session->client_fd);
        if (conn)
        {
            http::HttpResponse* error_response =
                request_handler_->createErrorResponse(500, conn);
            conn->SetResponse(error_response);
            conn->SetCgiSession(NULL);

            // クライアントFDを書き込みモードに
            FdSession* session =
                FdSession::create(cgi_session->client_fd, kWriteEvent, NULL);
            event_manager_->setWatchedEvents(session, kWriteEvent);
        }

        // CGI FDを削除
        event_manager_->remove(cgi_fd);
        if (cgi_session->cgi_input_fd >= 0)
        {
            event_manager_->remove(cgi_session->cgi_input_fd);
        }
        cgi_executor_->cleanupSession(cgi_session);
        return;
    }

    bool is_complete = read_result.unwrap();

    if (is_complete)
    {
        // CGI出力完了
        Log::info("CGI output complete for fd=", cgi_fd);

        // HttpResponseを作成
        Result<http::HttpResponse*> response_result =
            cgi_executor_->createResponseFromCgiOutput(cgi_session);

        ConnSocket* conn =
            connection_manager_->getConnection(cgi_session->client_fd);

        if (!response_result.isOk() || !conn)
        {
            Log::error("Failed to create response from CGI output");
            if (conn)
            {
                http::HttpResponse* error_response =
                    request_handler_->createErrorResponse(500, conn);
                conn->SetResponse(error_response);
                conn->SetCgiSession(NULL);
            }
        }
        else
        {
            conn->SetResponse(response_result.unwrap());
            conn->SetCgiSession(NULL);
        }

        // CGI FDを削除
        event_manager_->remove(cgi_fd);
        if (cgi_session->cgi_input_fd >= 0)
        {
            event_manager_->remove(cgi_session->cgi_input_fd);
        }
        cgi_executor_->cleanupSession(cgi_session);

        // クライアントFDを書き込みモードに
        if (conn)
        {
            FdSession session =
                FdSession::create(cgi_session->client_fd, kWriteEvent, NULL);
            event_manager_->setWatchedEvents(session);
        }
    }
}

// CGI入力イベント処理
void Server::handleCgiInputEvent(int cgi_input_fd, FdEventType events)
{
    if (!(events & kWriteEvent))
    {
        return;
    }

    CgiSession* cgi_session = cgi_executor_->getSessionByInputFd(cgi_input_fd);

    if (!cgi_session)
    {
        // 該当なし（おそらくクライアント接続の誤判定）
        Log::warning("Unknown fd in CGI input handler: ", cgi_input_fd);
        return;
    }

    // CGI入力を書き込み
    Result<bool> write_result = cgi_executor_->writeCgiInput(cgi_session);

    if (!write_result.isOk())
    {
        Log::error("CGI write error: ", write_result.getErrorMessage());

        // エラー時はCGIを中断
        event_manager_->remove(cgi_session->cgi_output_fd);
        event_manager_->remove(cgi_input_fd);
        cgi_executor_->cleanupSession(cgi_session);

        // クライアントにエラーレスポンス
        ConnSocket* conn =
            connection_manager_->getConnection(cgi_session->client_fd);
        if (conn)
        {
            http::HttpResponse* error_response =
                request_handler_->createErrorResponse(500, conn);
            conn->SetResponse(error_response);
            conn->SetCgiSession(NULL);

            FdSession session =
                FdSession::create(cgi_session->client_fd, kWriteEvent, NULL);
            event_manager_->setWatchedEvents(session);
        }
        return;
    }

    bool is_complete = write_result.unwrap();

    if (is_complete)
    {
        // 入力送信完了
        Log::info("CGI input complete for fd=", cgi_input_fd);

        // 入力FDを監視解除（すでにcloseされている）
        event_manager_->remove(cgi_input_fd);
    }
}

bool Server::handleParseError(ConnSocket* conn, HttpStatus status)
{
    Log::warning("Parse error: fd=", conn->GetFd(), ", status=", status);

    // エラーレスポンスを作成
    http::HttpResponse* error_response =
        request_handler_->createErrorResponse(status, conn);

    conn->SetResponse(error_response);

    // 書き込みモードに切り替え
    FdSession session = FdSession::create(conn->GetFd(), kWriteEvent, nullptr);
    event_manager_->setWatchedEvents(session);

    return false;  // エラーレスポンスを送ってから切断
}

bool Server::handleWrite(ConnSocket* conn)
{
    http::HttpResponse* response = conn->GetResponse();

    // レスポンスがなければ何もしない
    if (!response)
    {
        Log::warning("handleWrite called but no response: fd=", conn->GetFd());
        return false;
    }

    utils::ByteVector& response_data = response->GetResponseData();

    // 送信するデータがなければスキップ
    if (response_data.empty())
    {
        Log::warning("handleWrite called but response data is empty: fd=",
            conn->GetFd());
        return false;
    }

    // データを書き込み
    ssize_t n =
        write(conn->GetFd(), response_data.data(), response_data.size());

    if (n < 0)
    {
        // 一時的なエラー（非ブロッキングソケット）
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            // カーネルの送信バッファが満杯
            // 次の書き込み可能イベントを待つ
            return false;
        }

        // その他のエラー（接続切断など）
        Log::error("Write error on fd=", conn->GetFd(), ": ", strerror(errno));
        return true;  // 接続を閉じる
    }

    // 送信したデータをバッファから削除
    response_data.erase(response_data.begin(), response_data.begin() + n);

    Log::debug("Wrote ", n, " bytes to fd=", conn->GetFd());
    Log::debug("remaining=", response_data.size());

    // まだデータが残っていれば次の書き込みイベントを待つ
    if (!response_data.empty())
    {
        return false;
    }

    // ========== レスポンス送信完了 ==========

    Log::info("Response sent completely: fd=", conn->GetFd());

    // レスポンスオブジェクトを削除
    delete response;
    conn->SetResponse(nullptr);

    // 処理済みリクエストを削除
    std::deque<http::HttpRequest>& requests = conn->GetRequests();
    if (!requests.empty())
    {
        requests.pop_front();
    }

    // Connection: close の場合は接続を閉じる
    if (conn->IsShutdown())
    {
        Log::info("Connection: close, shutting down: fd=", conn->GetFd());
        return true;  // 接続を閉じる
    }

    // 次のリクエストがあれば処理
    // if (conn->HasParsedRequest())
    // {
    // 	http::HttpRequest &request = requests.front();
    // 	http::HttpResponse *new_response = new http::HttpResponse(request,
    // conn); 	conn->SetResponse(new_response); 	return false;
    // }
    // 次のリクエストが待機中か確認（パイプライニング）
    if (!requests.empty() && requests.front().isParseComplete())
    {
        Log::info("Processing next pipelined request: fd=", conn->GetFd());

        // 次のリクエストを即座に処理
        http::HttpRequest& next_request = requests.front();
        http::HttpResponse* next_response =
            request_handler_->handleRequest(next_request, conn, config_);

        conn->SetResponse(next_response);

        // 書き込みモード継続（イベント変更不要）
        return false;
    }

    // リクエストがもうなければ読み込みモードに戻る
    Log::debug("Switching to read mode: fd=", conn->GetFd());

    FdSession session = FdSession::create(conn->GetFd(), kReadEvent, nullptr);
    Result<void> result = event_manager_->setWatchedEvents(session);

    if (result.IsErr())
    {
        Log::error("Failed to switch to read mode: fd=", conn->GetFd(), ", ",
            result.getErrorMessage());
        return true;  // エラーなので接続を閉じる
    }

    return false;  // 接続継続
}

void Server::cleanup()
{
    // 全接続を閉じる
    connection_manager_->closeAll();
    listener_manager_->closeAll();
}

// 接続を閉じる
void Server::closeConnection(int fd)
{
    Log::info("Closing connection: fd=", fd);

    // 1. タイムアウトマネージャーから削除
    timeout_manager_->removeTimeout(fd);

    // 2. FdEventReactorから削除
    event_manager_->remove(fd);

    // 3. ConnectionManagerから削除（close + delete）
    connection_manager_->removeConnection(fd);
}

}  // namespace server