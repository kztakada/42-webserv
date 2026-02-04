#!/usr/bin/python3
import http.client
import re
import sys
from typing import Dict, List, Tuple


def request(host: str, port: int, path: str, headers: Dict[str, str]) -> Tuple[int, List[Tuple[str, str]], str]:
    conn = http.client.HTTPConnection(host, port, timeout=3)
    conn.request('GET', path, headers=headers)
    resp = conn.getresponse()
    body = resp.read().decode('utf-8', errors='replace')
    hdrs = resp.getheaders()  # list of (name, value)
    conn.close()
    return resp.status, hdrs, body


def pick_set_cookie(hdrs: List[Tuple[str, str]], cookie_name: str) -> str:
    # Set-Cookie は同名 cookie に対して複数行出ることがあるため、
    # まず Max-Age を含むものを優先し、なければ先頭一致を返す。
    fallback = ''
    for (k, v) in hdrs:
        if k.lower() != 'set-cookie':
            continue
        if not v.startswith(cookie_name + '='):
            continue
        if 'Max-Age=' in v:
            return v
        if fallback == '':
            fallback = v
    return fallback


def parse_cookie_value_from_set_cookie(set_cookie_value: str, cookie_name: str) -> str:
    # "NAME=value; ..."
    if not set_cookie_value.startswith(cookie_name + '='):
        return ''
    s = set_cookie_value[len(cookie_name) + 1 :]
    if ';' in s:
        s = s.split(';', 1)[0]
    return s


def assert_contains(s: str, needle: str) -> None:
    if needle not in s:
        raise AssertionError(f'missing "{needle}"')


def run_one(base_path: str, cookie_name: str) -> None:
    host = '127.0.0.1'
    port = 18088

    # 1st request: should issue cookie with Max-Age
    st, hdrs, body = request(host, port, base_path, headers={})
    if st != 200:
        raise AssertionError(f'expected 200, got {st}')

    set_cookie = pick_set_cookie(hdrs, cookie_name)
    if set_cookie == '':
        raise AssertionError('missing Set-Cookie')
    if 'Max-Age=' not in set_cookie:
        raise AssertionError('missing Max-Age in Set-Cookie')

    cid = parse_cookie_value_from_set_cookie(set_cookie, cookie_name)
    if cid == '':
        raise AssertionError('missing cookie id in Set-Cookie')

    assert_contains(body, 'new=1')
    assert_contains(body, 'id=' + cid)

    # 2nd request: send Cookie header; should reuse id
    cookie_hdr = f'{cookie_name}={cid}'
    st2, hdrs2, body2 = request(host, port, base_path, headers={'Cookie': cookie_hdr})
    if st2 != 200:
        raise AssertionError(f'expected 200, got {st2}')

    assert_contains(body2, 'new=0')
    assert_contains(body2, 'id=' + cid)


def main() -> int:
    try:
        run_one('/cgi/cookie.py', 'WEBSERV_ID')
        run_one('/cgi/cookie.php', 'WEBSERV_ID')
    except Exception as e:
        print('cookie test failed:', e, file=sys.stderr)
        return 1

    print('cookie test ok')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
