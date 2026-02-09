#!/usr/bin/bash

# sleep before sending CGI headers
sleep 20

printf 'Content-Type: text/plain\r\n'
printf '\r\n'
printf 'ok after sleep before headers\n'
