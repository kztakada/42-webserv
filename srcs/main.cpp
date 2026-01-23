#include <iostream>

#include "server/config/parser/config_parser.hpp"
#include "server/server.hpp"
#include "utils/result.hpp"

using namespace server;
using namespace utils::result;

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << " <config_file>" << std::endl;
        return 1;
    }

    try
    {
        // 1. 設定ファイルを読み込んでServerConfigオブジェクトを作成
        Result<ServerConfig> config_result = ConfigParser::parse(argv[1]);
        if (config_result.isError())
        {
            std::cerr << "Config error: " << config_result.getErrorMessage()
                      << std::endl;
            return 1;
        }

        const ServerConfig& config = config_result.unwrap();

        // 2. Serverインスタンスを作成（初期化済み）
        Result<Server*> server_result = Server::create(config);
        if (server_result.isError())
        {
            std::cerr << "Server creation failed: "
                      << server_result.getErrorMessage() << std::endl;
            return 1;
        }

        Server* server = server_result.unwrap();

        // 3. イベントループ開始（SIGINT/SIGTERMまでブロック）
        server->start();

        // 4. クリーンアップ
        delete server;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
