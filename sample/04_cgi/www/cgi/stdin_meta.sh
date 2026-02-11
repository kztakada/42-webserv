#!/usr/bin/bash

set -eu

tmp=$(mktemp /tmp/webserv_cgi_stdin_meta_XXXXXX.bin)
cleanup() { rm -f "$tmp"; }
trap cleanup EXIT

cat >"$tmp"
len=$(wc -c <"$tmp" | tr -d '[:space:]')
method=${REQUEST_METHOD:-}

printf 'Content-Type: text/plain\r\n'
printf '\r\n'
printf 'bash meta\n'
printf 'method=%s\n' "$method"
printf 'body_len=%s\n' "$len"
