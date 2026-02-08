#!/usr/bin/env python3
import time
import sys

time.sleep(5)
sys.stdout.write("Content-Type: text/plain\r\n\r\n")
sys.stdout.write("CGI Done\n")
sys.stdout.flush()
