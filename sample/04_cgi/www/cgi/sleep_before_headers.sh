#!/usr/bin/bash

# CGI that sleeps before sending headers
# Intentionally delays response headers to test CGI timeout behavior.

sleep 20

printf 'Content-Type: text/plain\r\n'
printf '\r\n'
printf 'slept before headers\n'
