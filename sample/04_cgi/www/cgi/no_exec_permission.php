#!/usr/bin/php
<?php
// このスクリプトは smoke で実行権限を外し、403 を確認するためのダミー。
// もし実行されたとしても、通常の CGI レスポンスを返す。
echo "Content-Type: text/plain\r\n\r\n";
echo "no_exec_permission.php\n";
?>
