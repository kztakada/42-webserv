#!/usr/bin/python3
import os
import sys


def main():
    body = sys.stdin.read()
    method = os.environ.get('REQUEST_METHOD', '')
    qs = os.environ.get('QUERY_STRING', '')

    sys.stdout.write('Content-Type: text/plain\r\n')
    sys.stdout.write('\r\n')
    sys.stdout.write('python ok\n')
    sys.stdout.write('method=' + method + '\n')
    sys.stdout.write('query=' + qs + '\n')
    sys.stdout.write('body=' + body)


if __name__ == '__main__':
    main()
