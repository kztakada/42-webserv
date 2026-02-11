<?php
$body = file_get_contents('php://stdin');
$method = getenv('REQUEST_METHOD');
$qs = getenv('QUERY_STRING');

if ($method === false) $method = '';
if ($qs === false) $qs = '';

echo "Content-Type: text/plain\r\n\r\n";
echo "php ok\n";
echo "method=" . $method . "\n";
echo "query=" . $qs . "\n";
echo "body=" . $body;
?>
