<?php
// 注意: sample の webserv.conf では /usr/bin/php (cli) を CGI executor として使っています。
// cli SAPI は header()/setcookie() を stdout に自動出力しないため、ここでは CGI 形式の
// ヘッダを echo で直接出力します。
// (php-cgi を使う場合は、より通常の書き方に差し替え可能です)

function parse_cookie_id($cookie_header, $name) {
	$cookie_header = (string)$cookie_header;
	$parts = explode(';', $cookie_header);
	foreach ($parts as $p) {
		$p = trim($p);
		if ($p === '') continue;
		$eq = strpos($p, '=');
		if ($eq === false) continue;
		$k = trim(substr($p, 0, $eq));
		$v = trim(substr($p, $eq + 1));
		if ($k === $name) return $v;
	}
	return '';
}

$cookie = getenv('HTTP_COOKIE');
if ($cookie === false) $cookie = '';

$cookie_name = 'WEBSERV_ID';
$cid = parse_cookie_id($cookie, $cookie_name);
$is_new = false;

if ($cid === '') {
	$is_new = true;
	try {
		$cid = bin2hex(random_bytes(16));
	} catch (Exception $e) {
		$cid = uniqid('id_', true);
	}
}

// session_start() の代表例として「Cookieの値でセッションIDを固定して開始」
// (サーバ側が HTTP_COOKIE を渡していれば、2回目以降も同じIDで動かせる)
if ($cid !== '') {
	session_id($cid);
}
@session_start();
@session_write_close();

// CGI headers
echo "Content-Type: text/plain\r\n";
if ($is_new) {
	// Max-Age はブラウザ管理のみの想定
	echo "Set-Cookie: {$cookie_name}={$cid}; Max-Age=60; Path=/\r\n";
}
echo "\r\n";

// body
echo "lang=php\n";
echo "new=" . ($is_new ? '1' : '0') . "\n";
echo "id=" . $cid . "\n";
?>
