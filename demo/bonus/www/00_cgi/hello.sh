#!/usr/bin/bash

# 変数宣言
body=$(cat)
content_length=${CONTENT_LENGTH:-}
content_type=${CONTENT_TYPE:-}
gateway_interface=${GATEWAY_INTERFACE:-}
path_info=${PATH_INFO:-}
path_translated=${PATH_TRANSLATED:-}
query_string=${QUERY_STRING:-}
remote_addr=${REMOTE_ADDR:-}
request_method=${REQUEST_METHOD:-}
script_name=${SCRIPT_NAME:-}
server_name=${SERVER_NAME:-}
server_port=${SERVER_PORT:-}
server_protocol=${SERVER_PROTOCOL:-}
server_software=${SERVER_SOFTWARE:-}


# CGIヘッダー（必須）
printf 'Content-Type: text/plain\r\n'
printf '\r\n'

printf 'bash ok\n'
# 実行ディレクトリ
printf 'Current Working Directory: %s\n' "$(pwd)"

# 環境変数----------
# HTTPリクエストメソッド（GET, POST, DELETE）
printf 'REQUEST_METHOD=%s\n' "$request_method"
# リクエストが使用したプロトコルの名前とバージョン (e.g., "HTTP/1.1")
printf 'SERVER_PROTOCOL=%s\n' "$server_protocol"
# CGIのバージョン
printf 'GATEWAY_INTERFACE=%s\n' "$gateway_interface"
# CGIスクリプト名
printf 'SCRIPT_NAME=%s\n' "$script_name"
# サーバーのホスト名
printf 'SERVER_NAME=%s\n' "$server_name"
# サーバーのポート番号
printf 'SERVER_PORT=%s\n' "$server_port"
# クライアントのIPアドレス
printf 'REMOTE_ADDR=%s\n' "$remote_addr"
# リクエストのContent-Length
printf 'CONTENT_LENGTH=%s\n' "$content_length"
# リクエストのContent-Type
printf 'CONTENT_TYPE=%s\n' "$content_type"
# CGIスクリプトへのパス情報
printf 'PATH_INFO=%s\n' "$path_info"
# PATH_INFOを変換した、ファイルシステム上のパス
printf 'PATH_TRANSLATED=%s\n' "$path_translated"
# クエリ文字列
printf 'QUERY_STRING=%s\n' "$query_string"
# サーバーソフトウェアの名前とバージョン
printf 'SERVER_SOFTWARE=%s\n' "$server_software"

# リクエストbody
printf 'body=%s' "$body"
