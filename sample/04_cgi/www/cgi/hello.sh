#!/usr/bin/bash

# Minimal CGI script
body=$(cat)
method=${REQUEST_METHOD:-}
qs=${QUERY_STRING:-}

printf 'Content-Type: text/plain\r\n'
printf '\r\n'
printf 'bash ok\n'
printf 'method=%s\n' "$method"
printf 'query=%s\n' "$qs"
printf 'body=%s' "$body"
