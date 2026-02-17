#!/usr/bin/python3
import os
import sys


def main():
    # 変数宣言
    body = sys.stdin.read()
    auth_type = os.environ.get('AUTH_TYPE', '')
    content_length = os.environ.get('CONTENT_LENGTH', '')
    content_type = os.environ.get('CONTENT_TYPE', '')
    gateway_interface = os.environ.get('GATEWAY_INTERFACE', '')
    path_info = os.environ.get('PATH_INFO', '')
    path_translated = os.environ.get('PATH_TRANSLATED', '')
    query_string = os.environ.get('QUERY_STRING', '')
    remote_addr = os.environ.get('REMOTE_ADDR', '')
    remote_host = os.environ.get('REMOTE_HOST', '')
    remote_ident = os.environ.get('REMOTE_IDENT', '')
    remote_user = os.environ.get('REMOTE_USER', '')
    request_method = os.environ.get('REQUEST_METHOD', '')
    script_name = os.environ.get('SCRIPT_NAME', '')
    server_name = os.environ.get('SERVER_NAME', '')
    server_port = os.environ.get('SERVER_PORT', '')
    server_protocol = os.environ.get('SERVER_PROTOCOL', '')
    server_software = os.environ.get('SERVER_SOFTWARE', '')

    # CGIヘッダー（必須）
    sys.stdout.write('Content-Type: text/plain\r\n')
    sys.stdout.write('\r\n')

    sys.stdout.write('python ok\n')
    # 実行ディレクトリ
    sys.stdout.write('Current Working Directory: ' + os.getcwd() + '\n')

    # 環境変数----------
    # // 必須の変数
    # HTTPリクエストメソッド（GET, POST, DELETE）
    sys.stdout.write('REQUEST_METHOD=' + request_method + '\n')
    # リクエストが使用したプロトコルの名前とバージョン (e.g., "HTTP/1.1")
    sys.stdout.write('SERVER_PROTOCOL=' + server_protocol + '\n')
    # CGIのバージョン
    sys.stdout.write('GATEWAY_INTERFACE=' + gateway_interface + '\n')
    # CGIスクリプト名
    sys.stdout.write('SCRIPT_NAME=' + script_name + '\n')
    # サーバーのホスト名
    sys.stdout.write('SERVER_NAME=' + server_name + '\n')
    # サーバーのポート番号
    sys.stdout.write('SERVER_PORT=' + server_port + '\n')
    # クライアントのIPアドレス
    sys.stdout.write('REMOTE_ADDR=' + remote_addr + '\n')

    # // POSTの場合のみ
    # リクエストのContent-Length
    sys.stdout.write('CONTENT_LENGTH=' + content_length + '\n')
    # リクエストのContent-Type
    sys.stdout.write('CONTENT_TYPE=' + content_type + '\n')

    # // PATH_INFO関連
    # CGIスクリプトへのパス情報
    sys.stdout.write('PATH_INFO=' + path_info + '\n')
    # PATH_INFOを変換した、ファイルシステム上のパス
    sys.stdout.write('PATH_TRANSLATED=' + path_translated + '\n')

    # クエリ文字列
    sys.stdout.write('QUERY_STRING=' + query_string + '\n')

    # サーバーソフトウェアの名前とバージョン
    sys.stdout.write('SERVER_SOFTWARE=' + server_software + '\n')



    # リクエストbody
    sys.stdout.write('body=' + body)

# このファイルを直接実行した場合のみ実行させる
if __name__ == '__main__':
    main()
