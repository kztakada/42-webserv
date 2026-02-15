#!/usr/bin/bash

# Minimal CGI script
body=$(cat)
method=${REQUEST_METHOD:-}
protocol=${SERVER_PROTOCOL:-}
script_name=${SCRIPT_NAME:-}
script_filename=${SCRIPT_FILENAME:-}
qs=${QUERY_STRING:-}
path=${PATH_INFO:-}
path_translated=${PATH_TRANSLATED:-}



printf 'Content-Type: text/plain\r\n'
printf '\r\n'
printf 'bash ok\n'
printf 'method=%s\n' "$method"
printf 'protocol=%s\n' "$protocol"
printf 'script_name=%s\n' "$script_name"
printf 'path=%s\n' "$path"
printf 'path_translated=%s\n' "$path_translated"
printf 'script_filename=%s\n' "$script_filename"
printf 'query=%s\n' "$qs"
printf 'body=%s' "$body"
