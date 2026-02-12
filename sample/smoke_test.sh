#!/usr/bin/env bash
set -euo pipefail # エラーが起きたら即座にスクリプトを止める(失敗/未定義変数の参照/パイプライン中のエラー)

cd "$(dirname "$0")/.." # webserv/へ移動

# 必須コマンドの存在確認
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
  local log="/tmp/webserv_sample_$$.log" # webserv起動後のstd_outを/tmpにlogとして保存する

  ./webserv "$config" >"$log" 2>&1 &
  local pid=$! # バックグラウンド実行pid

  cleanup() {
    if [ -n "${pid:-}" ]; then
      kill "$pid" 2>/dev/null || true
      wait "$pid" 2>/dev/null || true
    fi
  }
  trap cleanup EXIT INT TERM # cleanupトラップ

  sleep 0.2
  if ! kill -0 "$pid" 2>/dev/null; then # webserv生存確認
    echo "webserv exited early for $config" >&2
    cat "$log" >&2 || true
    trap - EXIT INT TERM
    cleanup
    return 1
  fi

  "$@"

  trap - EXIT INT TERM # cleanupトラップ終了
  cleanup
}



# --- 42_test ---
# case01
run_sample sample/42_test/webserv.conf bash -lc '
  set -e
  resp=$(curl -sS -I http://127.0.0.1:8080/)
  echo "$resp" | head -n1 | grep -Eq "^HTTP/1\\.[01] 405"
  echo "$resp" | grep -qi "Allow: GET"
'
# case02
run_sample sample/42_test/webserv.conf bash -lc '
  set -e
  resp=$(curl -sS -i http://127.0.0.1:8080/directory/Yeah)
  echo "$resp" | head -n1 | grep -Eq "^HTTP/1\\.[01] 404"
'

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

  # CGI script が実行可能であること（403 テスト導入に伴う前提）
  chmod 755 sample/02_error_page/www/cgi/loop.bad sample/02_error_page/www/cgi/bad_header.bad

  r=$(curl -sS -i http://127.0.0.1:18082/hello.txt)
  echo "$r" | head -n1 | grep -Eq "^HTTP/1\\.[01] 200"
  echo "$r" | grep -q "hello from sample/02_error_page"

  r=$(curl -sS -i -X POST --data "" http://127.0.0.1:18082/hello.txt)
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
  echo "$r" | head -n1 | grep -Eq "^HTTP/1\\.[01] 502"
  echo "$r" | grep -q "Custom 502"
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

  # CGI script が実行可能であること（403 テスト導入に伴う前提）
  chmod 755 sample/04_cgi/www/cgi/hello.py sample/04_cgi/www/cgi/hello.php sample/04_cgi/www/cgi/hello.js sample/04_cgi/www/cgi/hello.sh
  chmod 755 sample/04_cgi/www/cgi/stdin_meta.py sample/04_cgi/www/cgi/stdin_meta.php sample/04_cgi/www/cgi/stdin_meta.js sample/04_cgi/www/cgi/stdin_meta.sh

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

  # --- CGI stdin binary: body 内に \0 を含んでも全文が CGI に渡る ---
  tmp_nul=$(mktemp /tmp/webserv_cgi_nul_XXXXXX.bin)
  cleanup_nul() { rm -f "$tmp_nul"; }
  trap cleanup_nul EXIT
  P="$tmp_nul" python3 - <<PY
import os
p=os.environ["P"]
with open(p,"wb") as f:
    f.write(b"abc\x00def\n")
PY

  # python
  r=$(curl -sS -i -X POST "http://127.0.0.1:18084/cgi/stdin_meta.py" --data-binary "@$tmp_nul")
  echo "$r" | head -n1 | grep -Eq "^HTTP/1\\.[01] 200"
  echo "$r" | grep -q "body_len=8"
  echo "$r" | grep -q "nul_bytes=1"

  # php
  r=$(curl -sS -i -X POST "http://127.0.0.1:18084/cgi/stdin_meta.php" --data-binary "@$tmp_nul")
  echo "$r" | head -n1 | grep -Eq "^HTTP/1\\.[01] 200"
  echo "$r" | grep -q "body_len=8"
  echo "$r" | grep -q "nul_bytes=1"

  # node
  r=$(curl -sS -i -X POST "http://127.0.0.1:18084/cgi/stdin_meta.js" --data-binary "@$tmp_nul")
  echo "$r" | head -n1 | grep -Eq "^HTTP/1\\.[01] 200"
  echo "$r" | grep -q "body_len=8"
  echo "$r" | grep -q "nul_bytes=1"

  # bash
  r=$(curl -sS -i -X POST "http://127.0.0.1:18084/cgi/stdin_meta.sh" --data-binary "@$tmp_nul")
  echo "$r" | head -n1 | grep -Eq "^HTTP/1\\.[01] 200"
  echo "$r" | grep -q "body_len=8"

  trap - EXIT
  cleanup_nul

  # --- CGI client_max_body_size: 超過は 413 ---
  tmp_big=$(mktemp /tmp/webserv_cgi_big_XXXXXX.bin)
  cleanup_big() { rm -f "$tmp_big"; }
  trap cleanup_big EXIT
  dd if=/dev/zero of="$tmp_big" bs=1K count=1 status=none

  # python
  r=$(curl -sS -i -X POST "http://127.0.0.1:18084/cgi_small/hello.py" --data-binary "@$tmp_big")
  echo "$r" | head -n1 | grep -Eq "^HTTP/1\\.[01] 413"

  # php
  r=$(curl -sS -i -X POST "http://127.0.0.1:18084/cgi_small/hello.php" --data-binary "@$tmp_big")
  echo "$r" | head -n1 | grep -Eq "^HTTP/1\\.[01] 413"

  # node
  r=$(curl -sS -i -X POST "http://127.0.0.1:18084/cgi_small/hello.js" --data-binary "@$tmp_big")
  echo "$r" | head -n1 | grep -Eq "^HTTP/1\\.[01] 413"

  # bash
  r=$(curl -sS -i -X POST "http://127.0.0.1:18084/cgi_small/hello.sh" --data-binary "@$tmp_big")
  echo "$r" | head -n1 | grep -Eq "^HTTP/1\\.[01] 413"

  trap - EXIT
  cleanup_big

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

  # --- CGI error/timeout cases ---
  # 1) headers 確定前の timeout -> 504
  r=$(curl -sS -i --max-time 15 "http://127.0.0.1:18084/cgi_bash/timeout_before_headers.sh")
  echo "$r" | head -n1 | grep -Eq "^HTTP/1\\.[01] 504"

  # 2) headers 確定後の timeout -> 504
  r=$(curl -sS -i --max-time 15 "http://127.0.0.1:18084/cgi_bash/timeout_after_headers.sh")
  echo "$r" | head -n1 | grep -Eq "^HTTP/1\\.[01] 504"

  # 3) CGI 異常終了 -> 502
  r=$(curl -sS -i --max-time 5 "http://127.0.0.1:18084/cgi_bash/abnormal_exit.sh")
  echo "$r" | head -n1 | grep -Eq "^HTTP/1\\.[01] 502"

  # 4) 存在しない CGI script -> 404
  r=$(curl -sS -i --max-time 5 "http://127.0.0.1:18084/cgi_bash/no_such_script_abcdef.sh")
  echo "$r" | head -n1 | grep -Eq "^HTTP/1\\.[01] 404"

  # 5) 実行権限が無い CGI script -> 403
  chmod 644 sample/04_cgi/www/cgi/no_exec_permission.sh
  r=$(curl -sS -i --max-time 5 "http://127.0.0.1:18084/cgi_bash/no_exec_permission.sh")
  echo "$r" | head -n1 | grep -Eq "^HTTP/1\\.[01] 403"

  chmod 644 sample/04_cgi/www/cgi/no_exec_permission.py
  r=$(curl -sS -i --max-time 5 "http://127.0.0.1:18084/cgi_py/no_exec_permission.py")
  echo "$r" | head -n1 | grep -Eq "^HTTP/1\\.[01] 403"

  chmod 644 sample/04_cgi/www/cgi/no_exec_permission.php
  r=$(curl -sS -i --max-time 5 "http://127.0.0.1:18084/cgi_php/no_exec_permission.php")
  echo "$r" | head -n1 | grep -Eq "^HTTP/1\\.[01] 403"

  chmod 644 sample/04_cgi/www/cgi/no_exec_permission.js
  r=$(curl -sS -i --max-time 5 "http://127.0.0.1:18084/cgi_node/no_exec_permission.js")
  echo "$r" | head -n1 | grep -Eq "^HTTP/1\\.[01] 403"
'

# --- 05_upload_store ---
run_sample sample/05_upload_store/webserv_default_limit.conf bash -lc '
  set -euo pipefail

  store="sample/05_upload_store/store_default"
  dest_2m="$store/default_2m.bin"
  dest_512k="$store/default_512k.bin"
  dest_512k_multipart_png="$store/default_512k_multipart.png"
  dest_bodyname_png="$store/bodyname.png"
  dest_noct_bin="$store/noct.bin"
  dest_multi_a_png="$store/a.png"
  dest_multi_b_txt="$store/b.txt"
  dest_nul_raw="$store/nul_raw.bin"
  dest_nul_multipart_bin="$store/nul_multipart.bin"
  dest_overwrite="$store/overwrite.bin"
  tmp_small=$(mktemp /tmp/webserv_upload_512k_XXXXXX.bin)
  tmp_big=$(mktemp /tmp/webserv_upload_2m_XXXXXX.bin)
  tmp_small2=$(mktemp /tmp/webserv_upload_1k_XXXXXX.bin)
  tmp_txt=$(mktemp /tmp/webserv_upload_txt_XXXXXX.txt)
  tmp_nul=$(mktemp /tmp/webserv_upload_nul_XXXXXX.bin)
  tmp_mp_noct=$(mktemp /tmp/webserv_multipart_noct_XXXXXX.bin)
  tmp_mp_get=$(mktemp /tmp/webserv_multipart_get_XXXXXX.bin)
  tmp_overwrite_1=$(mktemp /tmp/webserv_upload_overwrite1_XXXXXX.bin)
  tmp_overwrite_2=$(mktemp /tmp/webserv_upload_overwrite2_XXXXXX.bin)

  cleanup_upload() {
    rm -f "$tmp_small" "$tmp_big" "$tmp_small2" "$tmp_txt" "$tmp_nul" "$tmp_mp_noct" "$tmp_mp_get"
    rm -f "$tmp_overwrite_1" "$tmp_overwrite_2"
    rm -f "$dest_2m" "$dest_512k" "$dest_512k_multipart_png" "$dest_bodyname_png" "$dest_noct_bin" "$dest_multi_a_png" "$dest_multi_b_txt" "$dest_nul_raw" "$dest_nul_multipart_bin" "$dest_overwrite"
    rm -f "$store"/*_uploaded*.bin 2>/dev/null || true
    rm -f "$store"/*_uploaded*.png 2>/dev/null || true
  }
  trap cleanup_upload EXIT

  # デフォルト(1MB)想定: 2MB は 413
  dd if=/dev/zero of="$tmp_big" bs=1M count=2 status=none
  before_count=$(find "$store" -maxdepth 1 -type f ! -name ".keep" | wc -l | tr -d "[:space:]")
  r=$(curl -sS -i -X POST --data-binary "@$tmp_big" http://127.0.0.1:18085/upload/default_2m.bin)
  echo "$r" | head -n1 | grep -Eq "^HTTP/1\\.[01] 413"
  test ! -f "$dest_2m"
  after_count=$(find "$store" -maxdepth 1 -type f ! -name ".keep" | wc -l | tr -d "[:space:]")
  test "$before_count" -eq "$after_count"

  # multipart/form-data でも client_max_body_size を超えたら 413 になる
  before_count=$(find "$store" -maxdepth 1 -type f ! -name ".keep" | wc -l | tr -d "[:space:]")
  r=$(curl -sS -i -X POST -F "file=@$tmp_big;filename=too_big;type=image/png" http://127.0.0.1:18085/upload/)
  echo "$r" | head -n1 | grep -Eq "^HTTP/1\\.[01] 413"
  after_count=$(find "$store" -maxdepth 1 -type f ! -name ".keep" | wc -l | tr -d "[:space:]")
  test "$before_count" -eq "$after_count"

  # 512KB は成功（POST -> 201）
  dd if=/dev/zero of="$tmp_small" bs=1K count=512 status=none
  r=$(curl -sS -i -X POST --data-binary "@$tmp_small" http://127.0.0.1:18085/upload/default_512k.bin)
  echo "$r" | head -n1 | grep -Eq "^HTTP/1\\.[01] 201"
  test -f "$dest_512k"

  size_dest=$(wc -c <"$dest_512k" | tr -d "[:space:]")
  size_src=$(wc -c <"$tmp_small" | tr -d "[:space:]")
  test "$size_dest" -eq "$size_src"

  # multipart/form-data: filename と Content-Type から保存ファイル名/拡張子が決まる
  r=$(curl -sS -i -X POST -F "file=@$tmp_small;filename=default_512k_multipart;type=image/png" http://127.0.0.1:18085/upload/)
  echo "$r" | head -n1 | grep -Eq "^HTTP/1\\.[01] 201"
  test -f "$dest_512k_multipart_png"

  size_dest=$(wc -c <"$dest_512k_multipart_png" | tr -d "[:space:]")
  size_src=$(wc -c <"$tmp_small" | tr -d "[:space:]")
  test "$size_dest" -eq "$size_src"

  # multipart/form-data: filename 指定が無い(空)場合、ユニークなデフォルト名が適用される
  rm -f "$store"/*_uploaded*.png 2>/dev/null || true
  r=$(curl -sS -i -X POST -F "file=@$tmp_small;filename=;type=image/png" http://127.0.0.1:18085/upload/)
  echo "$r" | head -n1 | grep -Eq "^HTTP/1\\.[01] 201"
  r=$(curl -sS -i -X POST -F "file=@$tmp_small;filename=;type=image/png" http://127.0.0.1:18085/upload/)
  echo "$r" | head -n1 | grep -Eq "^HTTP/1\\.[01] 201"
  count=$(ls -1 "$store" | grep -E "^[0-9]{14}_uploaded(_[0-9]+)?\\.png$" | wc -l | tr -d "[:space:]")
  test "$count" -eq 2

  # multipart/form-data: Content-Type が無い場合、デフォルト拡張子(bin)が適用される
  rm -f "$dest_noct_bin" 2>/dev/null || true
  boundary="----webserv_noct_$RANDOM$RANDOM"
  BOUNDARY="$boundary" SRC="$tmp_small" DST="$tmp_mp_noct" python3 - <<PY
import os
boundary = os.environ["BOUNDARY"]
src = os.environ["SRC"]
dst = os.environ["DST"]
with open(src, "rb") as f:
    data = f.read()
head = (
    "--" + boundary + "\r\n"
    "Content-Disposition: form-data; name=\"file\"; filename=\"noct\"\r\n"
    "\r\n"
).encode("ascii")
tail = ("\r\n--" + boundary + "--\r\n").encode("ascii")
with open(dst, "wb") as f:
    f.write(head)
    f.write(data)
    f.write(tail)
PY
  r=$(curl -sS -i -X POST -H "Content-Type: multipart/form-data; boundary=$boundary" --data-binary "@$tmp_mp_noct" http://127.0.0.1:18085/upload/)
  echo "$r" | head -n1 | grep -Eq "^HTTP/1\\.[01] 201"
  test -f "$dest_noct_bin"
  size_dest=$(wc -c <"$dest_noct_bin" | tr -d "[:space:]")
  size_src=$(wc -c <"$tmp_small" | tr -d "[:space:]")
  test "$size_dest" -eq "$size_src"

  # multipart/form-data: リクエスト先の保存ファイル名より、ボディ指定 filename/Content-Type を優先する
  rm -f "$dest_bodyname_png" 2>/dev/null || true
  r=$(curl -sS -i -X POST -F "file=@$tmp_small;filename=bodyname.txt;type=image/png" http://127.0.0.1:18085/upload/request.pdf)
  echo "$r" | head -n1 | grep -Eq "^HTTP/1\\.[01] 201"
  test -f "$dest_bodyname_png"

  # multipart/form-data: 異なる拡張子のファイルを同時にアップロードできる
  rm -f "$dest_multi_a_png" "$dest_multi_b_txt" 2>/dev/null || true
  printf "hello\n" >"$tmp_txt"
  dd if=/dev/zero of="$tmp_small2" bs=1K count=1 status=none
  r=$(curl -sS -i -X POST \
    -F "f1=@$tmp_small2;filename=a;type=image/png" \
    -F "f2=@$tmp_txt;filename=b;type=text/plain" \
    http://127.0.0.1:18085/upload/)
  echo "$r" | head -n1 | grep -Eq "^HTTP/1\\.[01] 201"
  test -f "$dest_multi_a_png"
  test -f "$dest_multi_b_txt"
  test "$(wc -c <"$dest_multi_a_png" | tr -d "[:space:]")" -eq "$(wc -c <"$tmp_small2" | tr -d "[:space:]")"
  test "$(wc -c <"$dest_multi_b_txt" | tr -d "[:space:]")" -eq "$(wc -c <"$tmp_txt" | tr -d "[:space:]")"

  # 保存データ途中に \0 が含まれていても保存できる（raw / multipart）
  P="$tmp_nul" python3 - <<PY
import os
p=os.environ["P"]
with open(p,"wb") as f:
    f.write(b"abc\x00def\n")
PY
  rm -f "$dest_nul_raw" 2>/dev/null || true
  r=$(curl -sS -i -X POST --data-binary "@$tmp_nul" http://127.0.0.1:18085/upload/nul_raw.bin)
  echo "$r" | head -n1 | grep -Eq "^HTTP/1\\.[01] 201"
  test -f "$dest_nul_raw"
  A="$tmp_nul" B="$dest_nul_raw" python3 - <<PY
import os, sys
a=os.environ["A"]
b=os.environ["B"]
da=open(a,"rb").read()
db=open(b,"rb").read()
sys.exit(0 if da==db else 1)
PY

  rm -f "$dest_nul_multipart_bin" 2>/dev/null || true
  r=$(curl -sS -i -X POST -F "file=@$tmp_nul;filename=nul_multipart;type=application/octet-stream" http://127.0.0.1:18085/upload/)
  echo "$r" | head -n1 | grep -Eq "^HTTP/1\\.[01] 201"
  test -f "$dest_nul_multipart_bin"
  A="$tmp_nul" B="$dest_nul_multipart_bin" python3 - <<PY
import os, sys
a=os.environ["A"]
b=os.environ["B"]
da=open(a,"rb").read()
db=open(b,"rb").read()
sys.exit(0 if da==db else 1)
PY

  # URL がディレクトリ指定の場合は timestamp_uploaded.bin で保存される
  dd if=/dev/zero of="$tmp_small" bs=1K count=1 status=none
  r=$(curl -sS -i -X POST --data-binary "@$tmp_small" http://127.0.0.1:18085/upload/)
  echo "$r" | head -n1 | grep -Eq "^HTTP/1\\.[01] 201"
  count=$(ls -1 "$store" | grep -E "^[0-9]{14}_uploaded(_[0-9]+)?\\.bin$" | wc -l | tr -d "[:space:]")
  test "$count" -eq 1

  # GET/DELETE で body を送っても upload_store されない（処理は各メソッドとして正常に返る）
  before_count=$(find "$store" -maxdepth 1 -type f ! -name ".keep" | wc -l | tr -d "[:space:]")
  r=$(curl -sS -i -X GET --data-binary "@$tmp_small" http://127.0.0.1:18085/)
  echo "$r" | head -n1 | grep -Eq "^HTTP/1\\.[01] 200"
  boundary_get="----webserv_get_$RANDOM$RANDOM"
  BOUNDARY="$boundary_get" DST="$tmp_mp_get" python3 - <<PY
import os
boundary=os.environ["BOUNDARY"]
dst=os.environ["DST"]
body=(
    "--"+boundary+"\r\n"
    "Content-Disposition: form-data; name=\"x\"\r\n"
    "\r\n"
    "abc\r\n"
    "--"+boundary+"--\r\n"
).encode("ascii")
open(dst,"wb").write(body)
PY
  r=$(curl -sS -i -X GET -H "Content-Type: multipart/form-data; boundary=$boundary_get" --data-binary "@$tmp_mp_get" http://127.0.0.1:18085/)
  echo "$r" | head -n1 | grep -Eq "^HTTP/1\\.[01] 200"
  after_count=$(find "$store" -maxdepth 1 -type f ! -name ".keep" | wc -l | tr -d "[:space:]")
  test "$before_count" -eq "$after_count"

  r=$(curl -sS -i -X DELETE --data-binary "@$tmp_small" http://127.0.0.1:18085/upload/overwrite.bin)
  echo "$r" | head -n1 | grep -Eq "^HTTP/1\\.[01] 405"
  r=$(curl -sS -i -X DELETE -H "Content-Type: multipart/form-data; boundary=$boundary_get" --data-binary "@$tmp_mp_get" http://127.0.0.1:18085/upload/overwrite.bin)
  echo "$r" | head -n1 | grep -Eq "^HTTP/1\\.[01] 405"
  after_count=$(find "$store" -maxdepth 1 -type f ! -name ".keep" | wc -l | tr -d "[:space:]")
  test "$before_count" -eq "$after_count"

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

# --- 05_upload_store: unwritable upload_store returns 403 (raw/multipart) ---
tmp_unwritable_store=$(mktemp -d /tmp/webserv_unwritable_store_XXXXXX)
tmp_unwritable_conf=$(mktemp /tmp/webserv_unwritable_conf_XXXXXX.conf)
chmod 555 "$tmp_unwritable_store"
cat >"$tmp_unwritable_conf" <<EOF
server {
  listen 127.0.0.1:18185;
  server_name localhost;
  root ./sample/05_upload_store/www;

  location / {
    allow_methods GET;
  }

  location /upload {
    allow_methods POST;
    upload_store $tmp_unwritable_store;
  }
}
EOF

run_sample "$tmp_unwritable_conf" bash -lc '
  set -euo pipefail
  tmp_small=$(mktemp /tmp/webserv_unwritable_small_XXXXXX.bin)
  cleanup() { rm -f "$tmp_small"; }
  trap cleanup EXIT

  dd if=/dev/zero of="$tmp_small" bs=1K count=1 status=none

  # raw: file target
  r=$(curl -sS -i -X POST --data-binary "@$tmp_small" http://127.0.0.1:18185/upload/a.bin)
  echo "$r" | head -n1 | grep -Eq "^HTTP/1\\.[01] 403"

  # raw: directory target
  r=$(curl -sS -i -X POST --data-binary "@$tmp_small" http://127.0.0.1:18185/upload/)
  echo "$r" | head -n1 | grep -Eq "^HTTP/1\\.[01] 403"

  # multipart/form-data
  r=$(curl -sS -i -X POST -F "file=@$tmp_small;filename=unwritable;type=image/png" http://127.0.0.1:18185/upload/)
  echo "$r" | head -n1 | grep -Eq "^HTTP/1\\.[01] 403"
'

chmod 755 "$tmp_unwritable_store" 2>/dev/null || true
rm -rf "$tmp_unwritable_store" "$tmp_unwritable_conf"

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

  # CGI script が実行可能であること（403 テスト導入に伴う前提）
  chmod 755 sample/06_cookie/www/cgi/cookie.php sample/06_cookie/www/cgi/cookie_setcookie.php

  python3 sample/06_cookie/test_cookie.py
'

# --- 07_delete ---
run_sample sample/07_delete/webserv.conf bash -lc '
  set -euo pipefail

  root="sample/07_delete/www"
  f_delete="$root/delete_me.txt"
  f_not_allowed="$root/not_allowed.txt"
  d_dir="$root/dir"
  d_protected="$root/protected"
  f_protected="$d_protected/nope.txt"

  cleanup_perms() {
    chmod 0755 "$d_protected" 2>/dev/null || true
    rm -f "$f_protected" 2>/dev/null || true
  }
  trap cleanup_perms EXIT

  # このスモークは繰り返し実行されるため、削除対象を毎回作り直す
  printf "delete me\n" >"$f_delete"

  # 405 優先: method が許可されていない場合は、ファイルの有無に関係なく 405
  r=$(curl -sS -i -X DELETE http://127.0.0.1:18089/does_not_exist.txt)
  echo "$r" | head -n1 | grep -Eq "^HTTP/1\\.[01] 405"

  r=$(curl -sS -i -X DELETE http://127.0.0.1:18089/not_allowed.txt)
  echo "$r" | head -n1 | grep -Eq "^HTTP/1\\.[01] 405"
  test -f "$f_not_allowed"

  # method が許可されている場合のみ削除できる（204）
  test -f "$f_delete"
  r=$(curl -sS -i -X DELETE http://127.0.0.1:18089/delete/delete_me.txt)
  echo "$r" | head -n1 | grep -Eq "^HTTP/1\\.[01] 204"
  test ! -f "$f_delete"

  # 許可されているが存在しない場合は 404
  r=$(curl -sS -i -X DELETE http://127.0.0.1:18089/delete/missing.txt)
  echo "$r" | head -n1 | grep -Eq "^HTTP/1\\.[01] 404"

  # 対象がディレクトリの場合は 403
  test -d "$d_dir"
  r=$(curl -sS -i -X DELETE http://127.0.0.1:18089/delete/dir)
  echo "$r" | head -n1 | grep -Eq "^HTTP/1\\.[01] 403"

  # 親ディレクトリに書き込み権限がない場合は 403（unlink は parent dir の権限が必要）
  printf "cant delete\n" >"$f_protected"
  chmod 0555 "$d_protected"
  r=$(curl -sS -i -X DELETE http://127.0.0.1:18089/delete/protected/nope.txt)
  echo "$r" | head -n1 | grep -Eq "^HTTP/1\\.[01] 403"
  test -f "$f_protected"
'
echo "07 sample smoke checks passed."
# --- 09_keep_alive ---
run_sample sample/09_keep_alive/webserv.conf bash -lc '
  set -euo pipefail
  chmod 755 sample/09_keep_alive/www/cgi/stream.py
  python3 sample/09_keep_alive/test_keep_alive.py
'

# --- 10_http1_0 ---
run_sample sample/10_http1_0/webserv.conf bash -lc '
  set -euo pipefail

  chmod 755 sample/10_http1_0/www/cgi/stream.py
  python3 sample/10_http1_0/test_http1_0.py
'

echo "All sample smoke checks passed."
# --- 08_signal_test ---
bash sample/08_signal_test/run.sh
