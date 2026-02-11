#!/usr/bin/node

// このスクリプトは smoke で実行権限を外し、403 を確認するためのダミー。
// もし実行されたとしても、通常の CGI レスポンスを返す。
process.stdout.write("Content-Type: text/plain\r\n\r\n");
process.stdout.write("no_exec_permission.js\n");
