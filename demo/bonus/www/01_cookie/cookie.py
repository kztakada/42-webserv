#!/usr/bin/python3
import os
import uuid


def parse_cookie_id(cookie_header: str, name: str) -> str:
    # Cookie: a=b; c=d
    parts = cookie_header.split(';')
    for part in parts:
        p = part.strip() # 後ろに続く空白削除
        if not p:
            continue
        if '=' not in p:
            continue
        k, v = p.split('=', 1) # 1は最大分割数
        if k.strip() == name:
            return v.strip()
    return ''


def main() -> None:
    cookie = os.environ.get('HTTP_COOKIE', '')
    cookie_name = 'WEBSERV_ID'
    cid = parse_cookie_id(cookie, cookie_name)

    is_new = False
    if cid == '':
        cid = str(uuid.uuid4()) # 122ビット分のランダムな値
        is_new = True

    # CGI headers
    os.write(1, b'Content-Type: text/plain\r\n')
    if is_new:
        # Max-Age: ブラウザ管理のみ（サーバ側で期限管理はしない想定）
        header = f'Set-Cookie: {cookie_name}={cid}; Max-Age=60; Path=/'
        os.write(1, header.encode('utf-8') + b'\r\n')
    os.write(1, b'\r\n')

    body = f'lang=py\nnew={1 if is_new else 0}\nid={cid}\n'
    os.write(1, body.encode('utf-8'))


if __name__ == '__main__':
    main()
