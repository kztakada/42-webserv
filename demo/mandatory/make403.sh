#!/usr/bin/env bash
cd "$(dirname "$0")"

# 1. ディレクトリを作成（既存の場合は何もしない）
mkdir -p ./www/05_custom_error/403

# 2. ファイルを作成し、文字列を書き込む
echo "You can't this file!" > ./www/05_custom_error/403/forbidden.txt

# 3. 読み込み権限を削除する
chmod -r ./www/05_custom_error/403/forbidden.txt