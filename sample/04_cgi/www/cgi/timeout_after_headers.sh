#!/usr/bin/bash

# send headers immediately, then hang before body
printf 'Content-Type: text/plain\r\n'
printf '\r\n'

sleep 20

printf 'body after sleep\n'
