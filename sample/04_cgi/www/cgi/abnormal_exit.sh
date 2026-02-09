#!/usr/bin/bash

# abnormal termination without CGI headers
printf 'abnormal exit\n' 1>&2
exit 1
