import time

# Intentionally never emits CGI headers.
# The server should hit its CGI timeout and return 504.
while True:
    time.sleep(1)
