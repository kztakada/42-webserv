#!/usr/bin/env python3
import sys
import time

# CGI response without Content-Length.
sys.stdout.write("Content-Type: text/plain\r\n\r\n")
sys.stdout.flush()

for i in range(5):
    sys.stdout.write("chunk-%d\n" % i)
    sys.stdout.flush()
    time.sleep(0.05)
