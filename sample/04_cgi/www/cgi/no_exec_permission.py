#!/usr/bin/python3

import sys

def main():
	# このスクリプトは smoke で実行権限を外し、403 を確認するためのダミー。
	# もし実行されたとしても、通常の CGI レスポンスを返す。
	sys.stdout.write("Content-Type: text/plain\r\n\r\n")
	sys.stdout.write("no_exec_permission.py\n")

if __name__ == "__main__":
	main()
