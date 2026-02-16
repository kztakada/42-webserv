#!/usr/bin/python3
import time

# 注意: ヘッダーも送らずに無限ループに入るパターン
# (サーバーがデータを待ち続けてブロックしないかテスト)
while True:
    time.sleep(1)