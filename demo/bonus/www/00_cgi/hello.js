// 標準モジュールである File System (fs) の読み込み
const fs = require("fs");

function readStdin() {
  try {
    return fs.readFileSync(0, "utf8"); // 0: 標準入力 (stdin) ,文字エンコーディング
  } catch (e) {
    return "";
  }
}

// process: 実行中のNode.jsプロセスに関する情報や制御（標準入出力、環境変数、終了処理など）するグローバルオブジェクト
const body = readStdin();
const content_length = process.env.CONTENT_LENGTH || "";
const content_type = process.env.CONTENT_TYPE || "";
const gateway_interface = process.env.GATEWAY_INTERFACE || "";
const path_info = process.env.PATH_INFO || "";
const path_translated = process.env.PATH_TRANSLATED || "";
const query_string = process.env.QUERY_STRING || "";
const remote_addr = process.env.REMOTE_ADDR || "";
const request_method = process.env.REQUEST_METHOD || "";
const script_name = process.env.SCRIPT_NAME || "";
const server_name = process.env.SERVER_NAME || "";
const server_port = process.env.SERVER_PORT || "";
const server_protocol = process.env.SERVER_PROTOCOL || "";
const server_software = process.env.SERVER_SOFTWARE || "";

process.stdout.write("Content-Type: text/plain\r\n");
process.stdout.write("\r\n");

process.stdout.write("node ok\n");
// 実行ディレクトリ
process.stdout.write("Current Working Directory: " + process.cwd() + "\n");

// 環境変数----------
// HTTPリクエストメソッド（GET, POST, DELETE）
process.stdout.write("REQUEST_METHOD=" + request_method + "\n");
// リクエストが使用したプロトコルの名前とバージョン (e.g., "HTTP/1.1")
process.stdout.write("SERVER_PROTOCOL=" + server_protocol + "\n");
// CGIのバージョン
process.stdout.write("GATEWAY_INTERFACE=" + gateway_interface + "\n");
// CGIスクリプト名
process.stdout.write("SCRIPT_NAME=" + script_name + "\n");
// サーバーのホスト名
process.stdout.write("SERVER_NAME=" + server_name + "\n");
// サーバーのポート番号
process.stdout.write("SERVER_PORT=" + server_port + "\n");
// クライアントのIPアドレス
process.stdout.write("REMOTE_ADDR=" + remote_addr + "\n");
// リクエストのContent-Length
process.stdout.write("CONTENT_LENGTH=" + content_length + "\n");
// リクエストのContent-Type
process.stdout.write("CONTENT_TYPE=" + content_type + "\n");
// CGIスクリプトへのパス情報
process.stdout.write("PATH_INFO=" + path_info + "\n");
// PATH_INFOを変換した、ファイルシステム上のパス
process.stdout.write("PATH_TRANSLATED=" + path_translated + "\n");
// クエリ文字列
process.stdout.write("QUERY_STRING=" + query_string + "\n");
// サーバーソフトウェアの名前とバージョン
process.stdout.write("SERVER_SOFTWARE=" + server_software + "\n");

// リクエストbody
process.stdout.write("body=" + body);
