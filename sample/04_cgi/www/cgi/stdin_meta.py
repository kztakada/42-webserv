#!/usr/bin/python3
import os
import sys


def main():
    data = sys.stdin.buffer.read()
    method = os.environ.get("REQUEST_METHOD", "")

    sys.stdout.write("Content-Type: text/plain\r\n")
    sys.stdout.write("\r\n")
    sys.stdout.write("python meta\n")
    sys.stdout.write("method=" + method + "\n")
    sys.stdout.write("body_len=" + str(len(data)) + "\n")
    sys.stdout.write("nul_bytes=" + str(data.count(b"\x00")) + "\n")


if __name__ == "__main__":
    main()
