<?php
// php-cgi 前提のサンプル:
// - session_start() / setcookie() を通常のPHP流儀で使う
// - 初回アクセスで WEBSERV_ID を発行（Max-Age付き）
// - 2回目は Cookie: WEBSERV_ID=... が渡れば同じIDを表示

$cookie_name = 'WEBSERV_ID';
$cid = '';
$is_new = false;

if (isset($_COOKIE[$cookie_name])) {
	$cid = (string)$_COOKIE[$cookie_name];
}

if ($cid === '') {
	$is_new = true;
	try {
		$cid = bin2hex(random_bytes(16));
	} catch (Exception $e) {
		$cid = uniqid('id_', true);
	}
}

// session_start() が動くことの確認（サーバ側が HTTP_COOKIE を渡していれば継続できる）
if ($cid !== '') {
	session_id($cid);
}
@session_start();
@session_write_close();

if ($is_new) {
	// まず通常の setcookie()
	@setcookie($cookie_name, $cid, time() + 60, '/');
	// さらに Max-Age を確実に含める（置換せず追加）
	header("Set-Cookie: {$cookie_name}={$cid}; Max-Age=60; Path=/", false);
}

header('Content-Type: text/plain');

echo "lang=php-cgi\n";
echo "new=" . ($is_new ? '1' : '0') . "\n";
echo "id=" . $cid . "\n";
?>
