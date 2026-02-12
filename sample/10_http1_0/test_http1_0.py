#!/usr/bin/env python3
import socket
import sys
import time

MAX_HEADER_SIZE = 16384


def read_until_header_end(sock):
    data = b""
    while b"\r\n\r\n" not in data:
        if len(data) > MAX_HEADER_SIZE:
            raise RuntimeError("Header size exceeded limit")
        chunk = sock.recv(1)
        if not chunk:
            break
        data += chunk
    return data


def read_exact(sock, length):
    data = b""
    while len(data) < length:
        chunk = sock.recv(min(4096, length - len(data)))
        if not chunk:
            break
        data += chunk
    return data


def parse_content_length(header_bytes):
    try:
        header_text = header_bytes.decode("iso-8859-1", errors="replace")
    except Exception:
        return None
    for line in header_text.split("\r\n"):
        if line.lower().startswith("content-length:"):
            value = line.split(":", 1)[1].strip()
            try:
                return int(value)
            except ValueError:
                return None
    return None


def header_has_chunked(header_bytes):
    return b"transfer-encoding: chunked" in header_bytes.lower()


def read_response(sock):
    header = read_until_header_end(sock)
    if not header:
        return b"", b""

    content_length = parse_content_length(header)
    if content_length is not None:
        body = read_exact(sock, content_length)
        return header, body

    # No Content-Length: read until close (expected for HTTP/1.0 CGI)
    body_parts = []
    while True:
        try:
            chunk = sock.recv(4096)
        except socket.timeout:
            break
        if not chunk:
            break
        body_parts.append(chunk)
    return header, b"".join(body_parts)


def assert_true(cond, msg):
    if not cond:
        raise AssertionError(msg)


def assert_contains(haystack, needle, msg):
    if needle not in haystack:
        raise AssertionError(msg)


def assert_not_contains(haystack, needle, msg):
    if needle in haystack:
        raise AssertionError(msg)


def test_http10_default_close(host, port):
    print("--- HTTP/1.0 no Host allowed + default close (static) ---")
    sock = socket.create_connection((host, port), timeout=2)
    sock.settimeout(2)
    try:
        req = b"GET /index.html HTTP/1.0\r\n\r\n"  # no Host
        sock.sendall(req)
        header, body = read_response(sock)
        assert_contains(header, b"200", "Expected 200 response")
        assert_not_contains(header.lower(), b"transfer-encoding: chunked", "chunked must not be used")
        assert_contains(body, b"hello from sample/10_http1_0", "Expected static body even without Host")

        # server should close connection by default for HTTP/1.0
        time.sleep(0.05)
        try:
            sock.sendall(b"GET /index.html HTTP/1.0\r\n\r\n")
        except Exception:
            print("OK: connection already closed")
            return

        sock.settimeout(0.3)
        h2, b2 = read_response(sock)
        assert_true(h2 == b"" and b2 == b"", "Expected connection closed (no second response)")
        print("OK: connection closed")
    finally:
        sock.close()


def test_http10_connection_close_explicit(host, port):
    print("--- HTTP/1.0 explicit Connection: close must disconnect ---")
    sock = socket.create_connection((host, port), timeout=2)
    sock.settimeout(2)
    try:
        req_keep = (
            b"GET /index.html HTTP/1.0\r\n"
            b"Connection: keep-alive\r\n"
            b"\r\n"
        )
        sock.sendall(req_keep)
        h1, b1 = read_response(sock)
        assert_contains(h1, b"200", "Expected 200 response")
        assert_not_contains(h1.lower(), b"transfer-encoding: chunked", "chunked must not be used")

        req_close = (
            b"GET /index.html HTTP/1.0\r\n"
            b"Connection: close\r\n"
            b"\r\n"
        )
        sock.sendall(req_close)
        h2, b2 = read_response(sock)
        assert_contains(h2, b"200", "Expected 200 response")
        assert_not_contains(h2.lower(), b"transfer-encoding: chunked", "chunked must not be used")

        sock.settimeout(0.3)
        tail = sock.recv(1)
        assert_true(tail == b"", "Expected server to close connection after explicit Connection: close")
        print("OK: disconnected after explicit close")
    finally:
        sock.close()


def test_http10_keep_alive_static(host, port):
    print("--- HTTP/1.0 keep-alive (static, 2 requests on same socket) ---")
    sock = socket.create_connection((host, port), timeout=2)
    sock.settimeout(2)
    try:
        req = (
            b"GET /index.html HTTP/1.0\r\n"
            b"Connection: keep-alive\r\n"
            b"\r\n"
        )
        sock.sendall(req)
        h1, b1 = read_response(sock)
        assert_contains(h1, b"200", "Expected 200 response")
        assert_not_contains(h1.lower(), b"transfer-encoding: chunked", "chunked must not be used")

        time.sleep(0.05)
        sock.sendall(req)
        h2, b2 = read_response(sock)
        assert_true(h2 != b"", "Expected second response on same connection")
        assert_contains(h2, b"200", "Expected 200 response")
        assert_not_contains(h2.lower(), b"transfer-encoding: chunked", "chunked must not be used")
        print("OK: keep-alive worked for static")
    finally:
        sock.close()


def test_http10_cgi_no_chunked_and_close(host, port, keep_alive):
    label = "keep-alive" if keep_alive else "no keep-alive"
    print("--- HTTP/1.0 CGI (unknown length): no chunked + close (%s) ---" % label)
    sock = socket.create_connection((host, port), timeout=2)
    sock.settimeout(1)
    try:
        headers = b""
        if keep_alive:
            headers += b"Connection: keep-alive\r\n"
        req = b"GET /cgi/stream.py HTTP/1.0\r\n" + headers + b"\r\n"
        sock.sendall(req)

        h, b = read_response(sock)
        assert_contains(h, b"200", "Expected 200 response")
        assert_not_contains(h.lower(), b"transfer-encoding: chunked", "chunked must not be used")

        # must close after sending when length is unknown (HTTP/1.0)
        # if already closed, recv returns b"" immediately
        sock.settimeout(0.3)
        tail = sock.recv(1)
        assert_true(tail == b"", "Expected server to close connection after CGI response")
        print("OK: closed after CGI and no chunked")
    finally:
        sock.close()


def test_http10_unsupported_method_is_405(host, port):
    print("--- HTTP/1.0 unsupported method => 405 ---")
    sock = socket.create_connection((host, port), timeout=2)
    sock.settimeout(2)
    try:
        req = b"HEAD /index.html HTTP/1.0\r\n\r\n"
        sock.sendall(req)
        h, b = read_response(sock)
        assert_contains(h, b"405", "Expected 405 for unsupported method")
        assert_contains(h.lower(), b"allow:", "Expected Allow header for 405")
        print("OK: 405 returned")
    finally:
        sock.close()


def main():
    host = "127.0.0.1"
    port = 18092

    test_http10_default_close(host, port)
    test_http10_keep_alive_static(host, port)
    test_http10_connection_close_explicit(host, port)
    test_http10_cgi_no_chunked_and_close(host, port, keep_alive=False)
    test_http10_cgi_no_chunked_and_close(host, port, keep_alive=True)
    test_http10_unsupported_method_is_405(host, port)

    print("All HTTP/1.0 tests passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
