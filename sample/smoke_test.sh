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

if ! command -v dd >/dev/null 2>&1; then
  echo "dd is required for upload_store sample" >&2
  exit 1
fi
if ! command -v mktemp >/dev/null 2>&1; then
  echo "mktemp is required for upload_store sample" >&2
  exit 1
fi

run_sample() {
  local config="$1"; shift
  local log="/tmp/webserv_sample_$$.log"

  ./webserv "$config" >"$log" 2>&1 &
  local pid=$!

  cleanup() {
    if [ -n "${pid:-}" ]; then
      kill "$pid" 2>/dev/null || true
      wait "$pid" 2>/dev/null || true
    fi
  }
  trap cleanup EXIT INT TERM

  sleep 0.2
  if ! kill -0 "$pid" 2>/dev/null; then
    echo "webserv exited early for $config" >&2
    cat "$log" >&2 || true
    trap - EXIT INT TERM
    cleanup
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

# --- 05_upload_store ---
run_sample sample/05_upload_store/webserv_default_limit.conf bash -lc '
  set -euo pipefail

  store="sample/05_upload_store/store_default"
  dest_2m="$store/default_2m.bin"
  dest_512k="$store/default_512k.bin"
  dest_overwrite="$store/overwrite.bin"
  tmp_small=$(mktemp /tmp/webserv_upload_512k_XXXXXX.bin)
  tmp_big=$(mktemp /tmp/webserv_upload_2m_XXXXXX.bin)
  tmp_overwrite_1=$(mktemp /tmp/webserv_upload_overwrite1_XXXXXX.bin)
  tmp_overwrite_2=$(mktemp /tmp/webserv_upload_overwrite2_XXXXXX.bin)

  cleanup_upload() {
    rm -f "$tmp_small" "$tmp_big"
    rm -f "$tmp_overwrite_1" "$tmp_overwrite_2"
    rm -f "$dest_2m" "$dest_512k" "$dest_overwrite"
  }
  trap cleanup_upload EXIT

  # デフォルト(1MB)想定: 2MB は 413
  dd if=/dev/zero of="$tmp_big" bs=1M count=2 status=none
  r=$(curl -sS -i -X POST --data-binary "@$tmp_big" http://127.0.0.1:18085/upload/default_2m.bin)
  echo "$r" | head -n1 | grep -Eq "^HTTP/1\\.[01] 413"
  test ! -f "$dest_2m"

  # 512KB は成功（POST -> 201）
  dd if=/dev/zero of="$tmp_small" bs=1K count=512 status=none
  r=$(curl -sS -i -X POST --data-binary "@$tmp_small" http://127.0.0.1:18085/upload/default_512k.bin)
  echo "$r" | head -n1 | grep -Eq "^HTTP/1\\.[01] 201"
  test -f "$dest_512k"

  size_dest=$(wc -c <"$dest_512k" | tr -d "[:space:]")
  size_src=$(wc -c <"$tmp_small" | tr -d "[:space:]")
  test "$size_dest" -eq "$size_src"

  # 同名ファイルが既にある場合は上書きされる（2回とも 201）
  printf "first\n" >"$tmp_overwrite_1"
  r=$(curl -sS -i -X POST --data-binary "@$tmp_overwrite_1" http://127.0.0.1:18085/upload/overwrite.bin)
  echo "$r" | head -n1 | grep -Eq "^HTTP/1\\.[01] 201"
  test -f "$dest_overwrite"
  test "$(wc -c <"$dest_overwrite" | tr -d "[:space:]")" -eq "$(wc -c <"$tmp_overwrite_1" | tr -d "[:space:]")"

  printf "second_is_longer\n" >"$tmp_overwrite_2"
  r=$(curl -sS -i -X POST --data-binary "@$tmp_overwrite_2" http://127.0.0.1:18085/upload/overwrite.bin)
  echo "$r" | head -n1 | grep -Eq "^HTTP/1\\.[01] 201"
  test -f "$dest_overwrite"
  test "$(wc -c <"$dest_overwrite" | tr -d "[:space:]")" -eq "$(wc -c <"$tmp_overwrite_2" | tr -d "[:space:]")"
'

run_sample sample/05_upload_store/webserv_inherit_server_limit.conf bash -lc '
  set -euo pipefail

  store="sample/05_upload_store/store_inherit"
  dest="$store/inherit_10m.bin"
  tmp_10m=$(mktemp /tmp/webserv_upload_10m_XXXXXX.bin)

  cleanup_upload() {
    rm -f "$tmp_10m"
    rm -f "$dest"
  }
  trap cleanup_upload EXIT

  # server の client_max_body_size(12M) を location が継承 → 10MB は成功
  dd if=/dev/zero of="$tmp_10m" bs=1M count=10 status=none
  r=$(curl -sS -i -X POST --data-binary "@$tmp_10m" http://127.0.0.1:18086/upload/inherit_10m.bin)
  echo "$r" | head -n1 | grep -Eq "^HTTP/1\\.[01] 201"
  test -f "$dest"

  size_dest=$(wc -c <"$dest" | tr -d "[:space:]")
  size_src=$(wc -c <"$tmp_10m" | tr -d "[:space:]")
  test "$size_dest" -eq "$size_src"
'

run_sample sample/05_upload_store/webserv_location_override.conf bash -lc '
  set -euo pipefail

  store="sample/05_upload_store/store_override"
  dest_small="$store/override_1m.bin"
  dest_small_plus1="$store/override_1m_plus1.bin"
  dest_big="$store/override_10m.bin"
  tmp_1m=$(mktemp /tmp/webserv_upload_1m_XXXXXX.bin)
  tmp_1m_plus1=$(mktemp /tmp/webserv_upload_1m_plus1_XXXXXX.bin)
  tmp_10m=$(mktemp /tmp/webserv_upload_10m_XXXXXX.bin)
  tmp_1k=$(mktemp /tmp/webserv_upload_1k_XXXXXX.bin)

  cleanup_upload() {
    rm -f "$tmp_1m" "$tmp_1m_plus1" "$tmp_10m" "$tmp_1k"
    rm -f "$dest_small" "$dest_small_plus1" "$dest_big"
  }
  trap cleanup_upload EXIT

  # location 上書き: 1,048,576 bytes ぴったりは OK
  dd if=/dev/zero of="$tmp_1m" bs=1M count=1 status=none
  r=$(curl -sS -i -X POST --data-binary "@$tmp_1m" http://127.0.0.1:18087/upload_small/override_1m.bin)
  echo "$r" | head -n1 | grep -Eq "^HTTP/1\\.[01] 201"
  test -f "$dest_small"
  test "$(wc -c <"$dest_small" | tr -d "[:space:]")" -eq 1048576

  # +1 は 413
  cp "$tmp_1m" "$tmp_1m_plus1"
  printf "\\0" >>"$tmp_1m_plus1"
  r=$(curl -sS -i -X POST --data-binary "@$tmp_1m_plus1" http://127.0.0.1:18087/upload_small/override_1m_plus1.bin)
  echo "$r" | head -n1 | grep -Eq "^HTTP/1\\.[01] 413"
  test ! -f "$dest_small_plus1"

  rm -f "$dest_small"

  # 別 location で上書き（20M）: 10MB は成功
  dd if=/dev/zero of="$tmp_10m" bs=1M count=10 status=none
  r=$(curl -sS -i -X POST --data-binary "@$tmp_10m" http://127.0.0.1:18087/upload_big/override_10m.bin)
  echo "$r" | head -n1 | grep -Eq "^HTTP/1\\.[01] 201"
  test -f "$dest_big"
  test "$(wc -c <"$dest_big" | tr -d "[:space:]")" -eq "$(wc -c <"$tmp_10m" | tr -d "[:space:]")"
'

# --- 06_cookie ---
run_sample sample/06_cookie/webserv.conf bash -lc '
  set -euo pipefail
  python3 sample/06_cookie/test_cookie.py
'

echo "All sample smoke checks passed."