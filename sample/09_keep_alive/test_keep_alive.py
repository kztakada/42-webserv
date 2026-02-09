#!/usr/bin/env python3
import socket
import sys
import time

MAX_HEADER_SIZE = 16384  # 16KB limit for headers to prevent infinite loops

def read_until_header_end(sock):
    data = b""
    while b"\r\n\r\n" not in data:
        if len(data) > MAX_HEADER_SIZE:
            raise RuntimeError(f"Header size exceeded limit ({MAX_HEADER_SIZE} bytes)")
        
        chunk = sock.recv(1) # Read byte by byte to avoid over-reading body/next response
        if not chunk:
            break
        data += chunk
    return data

def read_body(sock, length):
    data = b""
    while len(data) < length:
        chunk = sock.recv(min(4096, length - len(data)))
        if not chunk:
            break
        data += chunk
    return data

def read_response(sock):
    # Read headers
    header_data = read_until_header_end(sock)
    if not header_data:
        return b""

    # Parse Content-Length
    content_length = 0
    headers = header_data.decode(errors='ignore').split('\r\n')
    for h in headers:
        if h.lower().startswith("content-length:"):
            try:
                content_length = int(h.split(':', 1)[1].strip())
            except ValueError:
                pass
            break
            
    # Read body
    body_data = read_body(sock, content_length)
    return header_data + body_data

def test_keep_alive_success(host, port):
    print(f"--- Test 1: Standard Keep-alive Success ---")
    print(f"Connecting to {host}:{port}...")
    try:
        sock = socket.create_connection((host, port), timeout=2)
    except Exception as e:
        print(f"Failed to connect: {e}")
        return False

    try:
        # Request 1
        print("Sending Request 1...")
        req1 = b"GET /index.html HTTP/1.1\r\nHost: localhost\r\n\r\n"
        sock.sendall(req1)
        
        resp1 = read_response(sock)
        print(f"Response 1 received ({len(resp1)} bytes)")
        
        if b"200 OK" not in resp1:
            print("Error: Response 1 was not 200 OK")
            print(resp1.decode(errors='replace'))
            return False
            
        if b"Connection: close" in resp1:
            print("Error: Server sent 'Connection: close' in Response 1")
            return False
            
        # Small delay to ensure server has processed the response and is ready for next
        time.sleep(0.1)

        # Request 2 (Reuse same socket)
        print("Sending Request 2 (on same socket)...")
        req2 = b"GET /index.html HTTP/1.1\r\nHost: localhost\r\n\r\n"
        sock.sendall(req2)
        
        resp2 = read_response(sock)
        print(f"Response 2 received ({len(resp2)} bytes)")
        
        if len(resp2) == 0:
            print("Error: Response 2 was empty (Server closed connection?)")
            return False

        if b"200 OK" not in resp2:
            print("Error: Response 2 was not 200 OK")
            print(resp2.decode(errors='replace'))
            return False

        print("Test 1 Passed: Successfully received 2 responses on single socket.")
        return True

    except Exception as e:
        print(f"Exception during Test 1: {e}")
        return False
    finally:
        sock.close()

def test_crlf_bad_request(host, port):
    print(f"\n--- Test 2: Incomplete Request Must Not Respond ---")
    print(f"Connecting to {host}:{port}...")
    try:
        sock = socket.create_connection((host, port), timeout=2)
    except Exception as e:
        print(f"Failed to connect: {e}")
        return False

    try:
        # Send only CRLF (typical 'Enter' in telnet). Server should not respond.
        print("Sending incomplete input (CRLF)...")
        sock.sendall(b"\r\n")

        sock.settimeout(0.3)
        try:
            data = sock.recv(1)
            if data:
                print("Error: Server responded even though request is incomplete")
                return False
        except socket.timeout:
            # Expected: no response
            pass
        finally:
            sock.settimeout(2)

        # Now send a valid request on the same connection
        print("Sending valid request after incomplete input...")
        req = b"GET /index.html HTTP/1.1\r\nHost: localhost\r\n\r\n"
        sock.sendall(req)
        resp = read_response(sock)
        print(f"Response received ({len(resp)} bytes)")
        if b"200 OK" not in resp:
            print("Error: Response was not 200 OK")
            print(resp.decode(errors='replace'))
            return False

        print("Test 2 Passed: No response for incomplete input, then 200 OK.")
        return True

    except Exception as e:
        print(f"Exception during Test 2: {e}")
        return False
    finally:
        sock.close()

def main():
    host = '127.0.0.1'
    port = 18091
    
    success = True
    
    if not test_keep_alive_success(host, port):
        success = False
    
    if not test_crlf_bad_request(host, port):
        success = False
        
    if success:
        print("\nAll Keep-alive tests passed.")
        return 0
    else:
        print("\nSome Keep-alive tests failed.")
        return 1

if __name__ == "__main__":
    sys.exit(main())
