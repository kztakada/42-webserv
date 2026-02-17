<?php
// 変数宣言
$body = file_get_contents('php://stdin');
$content_length = getenv('CONTENT_LENGTH') ?: '';
$content_type = getenv('CONTENT_TYPE') ?: '';
$gateway_interface = getenv('GATEWAY_INTERFACE') ?: '';
$path_info = getenv('PATH_INFO') ?: '';
$path_translated = getenv('PATH_TRANSLATED') ?: '';
$query_string = getenv('QUERY_STRING') ?: '';
$remote_addr = getenv('REMOTE_ADDR') ?: '';
$request_method = getenv('REQUEST_METHOD') ?: '';
$script_name = getenv('SCRIPT_NAME') ?: '';
$server_name = getenv('SERVER_NAME') ?: '';
$server_port = getenv('SERVER_PORT') ?: '';
$server_protocol = getenv('SERVER_PROTOCOL') ?: '';
$server_software = getenv('SERVER_SOFTWARE') ?: '';

// CGIヘッダー（必須）
echo "Content-Type: text/plain\r\n\r\n";

echo "php ok\n";
// 実行ディレクトリ
echo "Current Working Directory: " . getcwd() . "\n";

// 環境変数----------
// HTTPリクエストメソッド（GET, POST, DELETE）
echo "REQUEST_METHOD=" . $request_method . "\n";
// リクエストが使用したプロトコルの名前とバージョン (e.g., "HTTP/1.1")
echo "SERVER_PROTOCOL=" . $server_protocol . "\n";
// CGIのバージョン
echo "GATEWAY_INTERFACE=" . $gateway_interface . "\n";
// CGIスクリプト名
echo "SCRIPT_NAME=" . $script_name . "\n";
// サーバーのホスト名
echo "SERVER_NAME=" . $server_name . "\n";
// サーバーのポート番号
echo "SERVER_PORT=" . $server_port . "\n";
// クライアントのIPアドレス
echo "REMOTE_ADDR=" . $remote_addr . "\n";
// リクエストのContent-Length
echo "CONTENT_LENGTH=" . $content_length . "\n";
// リクエストのContent-Type
echo "CONTENT_TYPE=" . $content_type . "\n";
// CGIスクリプトへのパス情報
echo "PATH_INFO=" . $path_info . "\n";
// PATH_INFOを変換した、ファイルシステム上のパス
echo "PATH_TRANSLATED=" . $path_translated . "\n";
// クエリ文字列
echo "QUERY_STRING=" . $query_string . "\n";
// サーバーソフトウェアの名前とバージョン
echo "SERVER_SOFTWARE=" . $server_software . "\n";

// リクエストbody
echo "body=" . $body;
?>
