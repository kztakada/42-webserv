#!/usr/bin/python3
import os
import sys


# 必須ヘッダー
print("Content-Type: text/plain\r\n\r\n", end="")

try:
    # 相対パス（ファイル名のみ）で開く
    with open("target.txt", "r") as f:
        print("Success: " + f.read())
except FileNotFoundError:
    print("Error: File not found using relative path.")
except Exception as e:
    print(f"Error: {e}")
