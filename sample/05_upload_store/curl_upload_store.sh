#!/usr/bin/env bash
set -euo pipefail

# 手動テスト用: upload_store の保存と client_max_body_size の挙動を curl で確認します。
# 事前に、別ターミナルで該当 conf で webserv を起動してください。

script_dir=$(cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(cd -- "$script_dir/../.." && pwd)

usage() {
  cat <<'USAGE'
Usage:
  bash sample/05_upload_store/curl_upload_store.sh default   # 18085 /upload
  bash sample/05_upload_store/curl_upload_store.sh inherit   # 18086 /upload
  bash sample/05_upload_store/curl_upload_store.sh override  # 18087 /upload_small,/upload_big

Note:
- 事前に `bash sample/05_upload_store/gen_white_jpg.sh` を実行して white.jpg を生成してください。
- このスクリプトは CWD がリポジトリルート(webserv/)である前提です。
USAGE
}

need_cmd() {
  command -v "$1" >/dev/null 2>&1 || { echo "missing command: $1" >&2; exit 1; }
}

need_cmd curl
need_cmd dd
need_cmd wc
need_cmd stat
need_cmd python3

if [[ $# -ne 1 ]]; then
  usage
  exit 2
fi

case "$1" in
  default)
    port=18085
    endpoint_base="http://127.0.0.1:${port}/upload"
    store_rel="sample/05_upload_store/store_default"
    ;;
  inherit)
    port=18086
    endpoint_base="http://127.0.0.1:${port}/upload"
    store_rel="sample/05_upload_store/store_inherit"
    ;;
  override)
    port=18087
    endpoint_small="http://127.0.0.1:${port}/upload_small"
    endpoint_big="http://127.0.0.1:${port}/upload_big"
    store_rel="sample/05_upload_store/store_override"
    ;;
  *)
    usage
    exit 2
    ;;
esac

if [[ "$PWD" != "$repo_root" ]]; then
  echo "please run from repo root: $repo_root" >&2
  exit 2
fi

base_jpg="$script_dir/fixtures/white.jpg"
if [[ ! -f "$base_jpg" ]]; then
  echo "missing: $base_jpg" >&2
  echo "run: bash sample/05_upload_store/gen_white_jpg.sh" >&2
  exit 1
fi

store="$repo_root/$store_rel"
if [[ ! -d "$store" ]]; then
  echo "missing store dir: $store" >&2
  exit 1
fi

tmp_dir=$(mktemp -d /tmp/webserv_upload_store_manual_XXXXXX)
cleanup() {
  rm -rf "$tmp_dir"
}
trap cleanup EXIT

make_payload_from_white_jpg() {
  local out="$1"
  local target_bytes="$2"

  cp "$base_jpg" "$out"

  local cur
  cur=$(stat -c%s "$out")
  if [[ "$cur" -gt "$target_bytes" ]]; then
    python3 - <<'PY'
import os
p=os.environ['P']
n=int(os.environ['N'])
raw=open(p,'rb').read(n)
open(p,'wb').write(raw)
PY
P="$out" N="$target_bytes"
    return 0
  fi

  local append=$((target_bytes - cur))
  if [[ "$append" -gt 0 ]]; then
    dd if=/dev/zero bs=1 count="$append" status=none >>"$out"
  fi

  local final
  final=$(stat -c%s "$out")
  [[ "$final" -eq "$target_bytes" ]]
}

assert_status() {
  local resp="$1"
  local code="$2"
  echo "$resp" | head -n1 | grep -Eq "^HTTP/1\\.[01] ${code}"
}

post_file() {
  local url="$1"
  local path="$2"
  curl -sS -i -X POST --data-binary "@${path}" "$url"
}

if [[ "$1" == "default" ]]; then
  # ディレクトリ宛の timestamp ファイルが残っているとテストが揺れるので先に掃除
  rm -f "$store"/*_uploaded*.bin 2>/dev/null || true

  # デフォルト(1MB)想定: 2MB は 413
  p2m="$tmp_dir/white_2m.jpg"
  make_payload_from_white_jpg "$p2m" 2097152
  dest_2m="$store/default_2m.jpg"
  rm -f "$dest_2m"
  r=$(post_file "$endpoint_base/default_2m.jpg" "$p2m")
  assert_status "$r" 413
  test ! -f "$dest_2m"

  # 512KB は 201
  p512k="$tmp_dir/white_512k.jpg"
  make_payload_from_white_jpg "$p512k" 524288
  dest_512k="$store/default_512k.jpg"
  rm -f "$dest_512k"
  r=$(post_file "$endpoint_base/default_512k.jpg" "$p512k")
  assert_status "$r" 201
  test -f "$dest_512k"
  test "$(wc -c <"$dest_512k" | tr -d '[:space:]')" -eq "$(wc -c <"$p512k" | tr -d '[:space:]')"

  # multipart/form-data: filename + Content-Type から保存ファイル名/拡張子が決まる
  dest_multipart_png="$store/default_512k_multipart.png"
  rm -f "$dest_multipart_png"
  r=$(curl -sS -i -X POST \
    -F "file=@$p512k;filename=default_512k_multipart;type=image/png" \
    "http://127.0.0.1:${port}/upload/")
  assert_status "$r" 201
  test -f "$dest_multipart_png"
  test "$(wc -c <"$dest_multipart_png" | tr -d '[:space:]')" -eq "$(wc -c <"$p512k" | tr -d '[:space:]')"

  # URL がディレクトリ指定の場合は timestamp_uploaded.bin で保存される
  p1k="$tmp_dir/raw_1k.bin"
  dd if=/dev/zero of="$p1k" bs=1K count=1 status=none
  r=$(post_file "http://127.0.0.1:${port}/upload/" "$p1k")
  assert_status "$r" 201
  count=$(ls -1 "$store" | grep -E "^[0-9]{14}_uploaded(_[0-9]+)?\\.bin$" | wc -l | tr -d "[:space:]")
  test "$count" -eq 1

  echo "OK: default"
  exit 0
fi

if [[ "$1" == "inherit" ]]; then
  # server の client_max_body_size(12M) を location が継承 → 10MB は 201
  p10m="$tmp_dir/white_10m.jpg"
  make_payload_from_white_jpg "$p10m" 10485760
  dest="$store/inherit_10m.jpg"
  rm -f "$dest"
  r=$(post_file "$endpoint_base/inherit_10m.jpg" "$p10m")
  assert_status "$r" 201
  test -f "$dest"
  test "$(wc -c <"$dest" | tr -d '[:space:]')" -eq "$(wc -c <"$p10m" | tr -d '[:space:]')"

  echo "OK: inherit"
  exit 0
fi

if [[ "$1" == "override" ]]; then
  # 1,048,576 bytes (= 1MiB) ぴったりは 201
  p1m="$tmp_dir/white_1m.jpg"
  make_payload_from_white_jpg "$p1m" 1048576
  dest_1m="$store/override_1m.jpg"
  rm -f "$dest_1m"
  r=$(post_file "$endpoint_small/override_1m.jpg" "$p1m")
  assert_status "$r" 201
  test -f "$dest_1m"
  test "$(wc -c <"$dest_1m" | tr -d '[:space:]')" -eq 1048576

  # +1 は 413
  p1m1="$tmp_dir/white_1m_plus1.jpg"
  make_payload_from_white_jpg "$p1m1" 1048577
  dest_1m1="$store/override_1m_plus1.jpg"
  rm -f "$dest_1m1"
  r=$(post_file "$endpoint_small/override_1m_plus1.jpg" "$p1m1")
  assert_status "$r" 413
  test ! -f "$dest_1m1"

  # upload_big(20M): 10MB は 201
  p10m="$tmp_dir/white_10m.jpg"
  make_payload_from_white_jpg "$p10m" 10485760
  dest_10m="$store/override_10m.jpg"
  rm -f "$dest_10m"
  r=$(post_file "$endpoint_big/override_10m.jpg" "$p10m")
  assert_status "$r" 201
  test -f "$dest_10m"
  test "$(wc -c <"$dest_10m" | tr -d '[:space:]')" -eq "$(wc -c <"$p10m" | tr -d '[:space:]')"

  echo "OK: override"
  exit 0
fi

usage
exit 2
