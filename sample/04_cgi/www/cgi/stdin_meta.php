#!/usr/bin/php
<?php
$stdin = fopen('php://stdin', 'rb');
$data = '';
while (!feof($stdin)) {
    $chunk = fread($stdin, 8192);
    if ($chunk === false) {
        break;
    }
    $data .= $chunk;
}
$len = strlen($data);
$nul = substr_count($data, "\0");
$method = isset($_SERVER['REQUEST_METHOD']) ? $_SERVER['REQUEST_METHOD'] : '';

echo "Content-Type: text/plain\r\n";
echo "\r\n";
echo "php meta\n";
echo "method=" . $method . "\n";
echo "body_len=" . $len . "\n";
echo "nul_bytes=" . $nul . "\n";
?>
