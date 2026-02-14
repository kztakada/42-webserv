#include <iostream>

#include "server/config/parser/config_parser.hpp"
#include "server/server.hpp"
#include "utils/result.hpp"

using namespace server;
using namespace utils::result;

int main(int argc, char* argv[])
{
    std::string config_file;
    if (argc == 1)
        config_file = "./srcs/server/config/default.conf";
    else if (argc == 2)
        config_file = argv[1];
    else
    {
        std::cerr << "Usage: " << argv[0] << " [configuration file]"
                  << std::endl;
        return EXIT_FAILURE;
    }

    try
    {
        // 1. 設定ファイルを読み込んでServerConfigオブジェクトを作成
        Result<ServerConfig> config_result =
            ConfigParser::parseFile(config_file);
        if (config_result.isError())
        {
            std::cerr << "Config error: " << config_result.getErrorMessage()
                      << std::endl;
            return EXIT_FAILURE;
        }

        const ServerConfig& config = config_result.unwrap();

        // 2. Serverインスタンスを作成（初期化済み）
        Result<Server*> server_result = Server::create(config);
        if (server_result.isError())
        {
            std::cerr << "Server creation failed: "
                      << server_result.getErrorMessage() << std::endl;
            return EXIT_FAILURE;
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
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
