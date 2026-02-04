#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

if ! command -v curl >/dev/null 2>&1; then
  echo "curl is required" >&2
  exit 1
fi

if ! command -v python3 >/dev/null 2>&1; then
  echo "python3 is required for CGI sample" >&2
  exit 1
fi
if ! command -v php >/dev/null 2>&1; then
  echo "php is required for CGI sample" >&2
  exit 1
fi
if ! command -v node >/dev/null 2>&1; then
  echo "node is required for CGI sample" >&2
  exit 1
fi
if ! command -v bash >/dev/null 2>&1; then
  echo "bash is required for CGI sample" >&2
  exit 1
fi

run_sample() {
  local config="$1"; shift
  local log="/tmp/webserv_sample_$$.log"

  ./webserv "$config" >"$log" 2>&1 &
  local pid=$!

  cleanup() {
    kill "$pid" 2>/dev/null || true
    wait "$pid" 2>/dev/null || true
  }
  trap cleanup EXIT INT TERM

  sleep 0.2
  if ! kill -0 "$pid" 2>/dev/null; then
    echo "webserv exited early for $config" >&2
    cat "$log" >&2 || true
    return 1
  fi

  "$@"

  trap - EXIT INT TERM
  cleanup
}

# --- 01_static ---
run_sample sample/01_static/webserv.conf bash -lc '
  set -e
  resp=$(curl -sS -i http://127.0.0.1:18081/hello.txt)
  echo "$resp" | head -n1 | grep -Eq "^HTTP/1\\.[01] 200"
  echo "$resp" | grep -q "hello from sample/01_static"
'

# --- 02_error_page ---
run_sample sample/02_error_page/webserv.conf bash -lc '
  set -e

  r=$(curl -sS -i http://127.0.0.1:18082/hello.txt)
  echo "$r" | head -n1 | grep -Eq "^HTTP/1\\.[01] 200"
  echo "$r" | grep -q "hello from sample/02_error_page"

  r=$(curl -sS -i -X POST http://127.0.0.1:18082/hello.txt)
  echo "$r" | head -n1 | grep -Eq "^HTTP/1\\.[01] 405"
  echo "$r" | grep -q "Custom 405"

  r=$(curl -sS -i http://127.0.0.1:18082/missing.txt)
  echo "$r" | head -n1 | grep -Eq "^HTTP/1\\.[01] 404"
  echo "$r" | grep -q "Custom 404"

  r=$(curl -sS -i http://127.0.0.1:18082/errors/)
  echo "$r" | head -n1 | grep -Eq "^HTTP/1\\.[01] 403"
  echo "$r" | grep -q "Custom 403"

  r=$(curl -sS -i --path-as-is http://127.0.0.1:18082/../secret)
  echo "$r" | head -n1 | grep -Eq "^HTTP/1\\.[01] 400"
  echo "$r" | grep -q "Custom 400"

  r=$(curl -sS -i http://127.0.0.1:18082/upload -H "Content-Type: text/plain" --data "0123456789")
  echo "$r" | head -n1 | grep -Eq "^HTTP/1\\.[01] 413"
  echo "$r" | grep -q "Custom 413"

  r=$(curl -sS -i http://127.0.0.1:18082/cgi/loop.bad)
  echo "$r" | head -n1 | grep -Eq "^HTTP/1\\.[01] 500"
  echo "$r" | grep -q "Custom 500"

  r=$(curl -sS -i http://127.0.0.1:18082/cgi/bad_header.bad)
  echo "$r" | head -n1 | grep -Eq "^HTTP/1\\.[01] 500"
  echo "$r" | grep -q "Custom 500"
'

# --- 03_autoindex ---
run_sample sample/03_autoindex/webserv.conf bash -lc '
  set -e

  r=$(curl -sS -i http://127.0.0.1:18083/dir/)
  echo "$r" | head -n1 | grep -Eq "^HTTP/1\\.[01] 200"
  echo "$r" | grep -qi "Content-Type: text/html"

  # directory listing body
  echo "$r" | grep -q "Index of /dir/"
  echo "$r" | grep -q "a.txt"
  echo "$r" | grep -q "sub/"
'

# --- 04_cgi ---
run_sample sample/04_cgi/webserv.conf bash -lc '
  set -e

  # python
  r=$(curl -sS -i "http://127.0.0.1:18084/cgi/hello.py?lang=py")
  echo "$r" | head -n1 | grep -Eq "^HTTP/1\\.[01] 200"
  echo "$r" | grep -qi "^Content-Type: text/plain"
  echo "$r" | grep -q "python ok"
  echo "$r" | grep -q "query=lang=py"

  r=$(curl -sS -i -X POST "http://127.0.0.1:18084/cgi/hello.py" --data "abc")
  echo "$r" | head -n1 | grep -Eq "^HTTP/1\\.[01] 200"
  echo "$r" | grep -q "body=abc"

  # php
  r=$(curl -sS -i "http://127.0.0.1:18084/cgi/hello.php?lang=php")
  echo "$r" | head -n1 | grep -Eq "^HTTP/1\\.[01] 200"
  echo "$r" | grep -qi "^Content-Type: text/plain"
  echo "$r" | grep -q "php ok"
  echo "$r" | grep -q "query=lang=php"

  r=$(curl -sS -i -X POST "http://127.0.0.1:18084/cgi/hello.php" --data "def")
  echo "$r" | head -n1 | grep -Eq "^HTTP/1\\.[01] 200"
  echo "$r" | grep -q "body=def"

  # node
  r=$(curl -sS -i "http://127.0.0.1:18084/cgi/hello.js?lang=node")
  echo "$r" | head -n1 | grep -Eq "^HTTP/1\\.[01] 200"
  echo "$r" | grep -qi "^Content-Type: text/plain"
  echo "$r" | grep -q "node ok"
  echo "$r" | grep -q "query=lang=node"

  r=$(curl -sS -i -X POST "http://127.0.0.1:18084/cgi/hello.js" --data "ghi")
  echo "$r" | head -n1 | grep -Eq "^HTTP/1\\.[01] 200"
  echo "$r" | grep -q "body=ghi"

  # bash
  r=$(curl -sS -i "http://127.0.0.1:18084/cgi/hello.sh?lang=bash")
  echo "$r" | head -n1 | grep -Eq "^HTTP/1\\.[01] 200"
  echo "$r" | grep -qi "^Content-Type: text/plain"
  echo "$r" | grep -q "bash ok"
  echo "$r" | grep -q "query=lang=bash"

  r=$(curl -sS -i -X POST "http://127.0.0.1:18084/cgi/hello.sh" --data "jkl")
  echo "$r" | head -n1 | grep -Eq "^HTTP/1\\.[01] 200"
  echo "$r" | grep -q "body=jkl"

  # locationごとの CGI 対象分離
  # /cgi_py は .py のみ CGI として動く
  r=$(curl -sS -i "http://127.0.0.1:18084/cgi_py/hello.py?only=py")
  echo "$r" | head -n1 | grep -Eq "^HTTP/1\\.[01] 200"
  echo "$r" | grep -q "python ok"
  echo "$r" | grep -q "query=only=py"

  r=$(curl -sS -i "http://127.0.0.1:18084/cgi_py/hello.php")
  echo "$r" | head -n1 | grep -Eq "^HTTP/1\\.[01] 200"
  echo "$r" | grep -q "<?php"

  # /cgi_php は .php のみ CGI として動く
  r=$(curl -sS -i "http://127.0.0.1:18084/cgi_php/hello.php?only=php")
  echo "$r" | head -n1 | grep -Eq "^HTTP/1\\.[01] 200"
  echo "$r" | grep -q "php ok"
  echo "$r" | grep -q "query=only=php"

  r=$(curl -sS -i "http://127.0.0.1:18084/cgi_php/hello.py")
  echo "$r" | head -n1 | grep -Eq "^HTTP/1\\.[01] 200"
  echo "$r" | grep -q "#!/usr/bin/python3"

  # /cgi_node は .js のみ CGI として動く
  r=$(curl -sS -i "http://127.0.0.1:18084/cgi_node/hello.js?only=node")
  echo "$r" | head -n1 | grep -Eq "^HTTP/1\\.[01] 200"
  echo "$r" | grep -q "node ok"
  echo "$r" | grep -q "query=only=node"

  r=$(curl -sS -i "http://127.0.0.1:18084/cgi_node/hello.sh")
  echo "$r" | head -n1 | grep -Eq "^HTTP/1\\.[01] 200"
  echo "$r" | grep -q "#!/usr/bin/bash"

  # /cgi_bash は .sh のみ CGI として動く
  r=$(curl -sS -i "http://127.0.0.1:18084/cgi_bash/hello.sh?only=bash")
  echo "$r" | head -n1 | grep -Eq "^HTTP/1\\.[01] 200"
  echo "$r" | grep -q "bash ok"
  echo "$r" | grep -q "query=only=bash"

  r=$(curl -sS -i "http://127.0.0.1:18084/cgi_bash/hello.js")
  echo "$r" | head -n1 | grep -Eq "^HTTP/1\\.[01] 200"
  echo "$r" | grep -q "const fs"
'

echo "All sample smoke checks passed."