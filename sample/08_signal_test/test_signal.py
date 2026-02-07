#!/usr/bin/env python3
import subprocess
import time
import os
import signal
import sys
import socket

def wait_for_port(port, timeout=10):
    start = time.time()
    while time.time() - start < timeout:
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=1):
                return True
        except (ConnectionRefusedError, socket.timeout):
            time.sleep(0.1)
    return False

def test_signal(sig, sig_name):
    print(f"Testing {sig_name}...")
    
    # Valgrind command
    # We use --error-exitcode=100 to detect memory errors
    cmd = [
        "valgrind",
        "--leak-check=full",
        "--show-leak-kinds=all",
        "--track-fds=yes",
        "--error-exitcode=100",
        "./webserv",
        "sample/08_signal_test/webserv.conf"
    ]
    
    # Start process
    proc = subprocess.Popen(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        cwd=os.getcwd(),
        preexec_fn=os.setsid # New process group to avoid killing test script
    )
    
    if not wait_for_port(18099):
        print("Server failed to start")
        proc.kill()
        stdout, stderr = proc.communicate()
        print("STDOUT:", stdout.decode())
        print("STDERR:", stderr.decode())
        return False

    print(f"Server started (PID {proc.pid}). Sending {sig_name}...")
    
    # Send signal
    os.kill(proc.pid, sig)
    
    try:
        stdout, stderr = proc.communicate(timeout=5)
    except subprocess.TimeoutExpired:
        print("Server failed to stop in time!")
        proc.kill()
        stdout, stderr = proc.communicate()
        print(stderr.decode())
        return False

    rc = proc.returncode
    print(f"Server exited with {rc}")
    
    stderr_str = stderr.decode()
    
    # Check for valgrind errors
    if rc == 100:
        print("Valgrind reported errors!")
        print(stderr_str)
        return False
        
    if "Open file descriptor" in stderr_str:
         if "socket" in stderr_str:
             print("Open socket descriptors detected!")
             print(stderr_str)
             return False

    # Check for leaks summary
    if "All heap blocks were freed" in stderr_str:
        pass
    elif "definitely lost: 0 bytes in 0 blocks" not in stderr_str:
        print("Possible leak detected (definitely lost != 0)")
        print(stderr_str)
        return False
        
    if "All heap blocks were freed" in stderr_str:
        pass
    elif "indirectly lost: 0 bytes in 0 blocks" not in stderr_str:
        print("Possible leak detected (indirectly lost != 0)")
        print(stderr_str)
        return False

    print(f"{sig_name} test passed.")
    return True

def test_signal_busy(sig, sig_name):
    print(f"Testing {sig_name} (Busy)...")
    
    cmd = [
        "valgrind",
        "--leak-check=full",
        "--show-leak-kinds=all",
        "--track-fds=yes",
        "--error-exitcode=100",
        "./webserv",
        "sample/08_signal_test/webserv.conf"
    ]
    
    proc = subprocess.Popen(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        cwd=os.getcwd(),
        preexec_fn=os.setsid
    )
    
    if not wait_for_port(18099):
        print("Server failed to start")
        proc.kill()
        stdout, stderr = proc.communicate()
        print("STDOUT:", stdout.decode())
        print("STDERR:", stderr.decode())
        return False

    print(f"Server started (PID {proc.pid}). sending partial request...")
    
    # Open connection and send partial data
    try:
        sock = socket.create_connection(("127.0.0.1", 18099), timeout=5)
        sock.sendall(b"GET / HTTP/1.1\r\nHost: localhost\r\n")
        # Do not send final \r\n, so server waits
        time.sleep(1)
    except Exception as e:
        print(f"Failed to connect/send: {e}")
        proc.kill()
        return False

    print(f"Sending {sig_name}...")
    os.kill(proc.pid, sig)
    
    try:
        stdout, stderr = proc.communicate(timeout=5)
    except subprocess.TimeoutExpired:
        print("Server failed to stop in time!")
        proc.kill()
        stdout, stderr = proc.communicate()
        print(stderr.decode())
        sock.close()
        return False

    sock.close()
    
    rc = proc.returncode
    print(f"Server exited with {rc}")
    
    stderr_str = stderr.decode()
    
    if rc == 100:
        print("Valgrind reported errors!")
        print(stderr_str)
        return False

    if "Open file descriptor" in stderr_str:
         if "socket" in stderr_str:
             print("Open socket descriptors detected!")
             print(stderr_str)
             return False

    if "All heap blocks were freed" in stderr_str:
        pass
    elif "definitely lost: 0 bytes in 0 blocks" not in stderr_str:
        print("Possible leak detected (definitely lost != 0)")
        print(stderr_str)
        return False
        
    if "All heap blocks were freed" in stderr_str:
        pass
    elif "indirectly lost: 0 bytes in 0 blocks" not in stderr_str:
        print("Possible leak detected (indirectly lost != 0)")
        print(stderr_str)
        return False

    print(f"{sig_name} (Busy) test passed.")
    return True

def test_signal_upload(sig, sig_name):
    print(f"Testing {sig_name} (Upload)...")
    
    cmd = [
        "valgrind",
        "--leak-check=full",
        "--show-leak-kinds=all",
        "--track-fds=yes",
        "--error-exitcode=100",
        "./webserv",
        "sample/08_signal_test/webserv.conf"
    ]
    
    proc = subprocess.Popen(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        cwd=os.getcwd(),
        preexec_fn=os.setsid
    )
    
    if not wait_for_port(18099):
        print("Server failed to start")
        proc.kill()
        stdout, stderr = proc.communicate()
        print("STDOUT:", stdout.decode())
        print("STDERR:", stderr.decode())
        return False

    print(f"Server started (PID {proc.pid}). sending large body...")

    # Open connection and send large body slowly
    try:
        sock = socket.create_connection(("127.0.0.1", 18099), timeout=10)
        sock.sendall(b"POST /upload HTTP/1.1\r\nHost: localhost\r\nContent-Length: 10485760\r\n\r\n")
        # Send chunks
        for _ in range(5):
            sock.sendall(b"A" * 1024 * 1024) # 1MB
            time.sleep(0.2)
    except Exception as e:
        print(f"Failed to connect/send: {e}")
        proc.kill()
        return False

    print(f"Sending {sig_name}...")
    os.kill(proc.pid, sig)
    
    try:
        stdout, stderr = proc.communicate(timeout=5)
    except subprocess.TimeoutExpired:
        print("Server failed to stop in time!")
        proc.kill()
        stdout, stderr = proc.communicate()
        print(stderr.decode())
        sock.close()
        return False

    sock.close()
    
    rc = proc.returncode
    print(f"Server exited with {rc}")
    
    stderr_str = stderr.decode()
    
    if rc == 100:
        print("Valgrind reported errors!")
        print(stderr_str)
        return False
        
    if "Open file descriptor" in stderr_str:
         if "socket" in stderr_str:
             print("Open socket descriptors detected!")
             print(stderr_str)
             return False

    if "All heap blocks were freed" in stderr_str:
        pass
    elif "definitely lost: 0 bytes in 0 blocks" not in stderr_str:
        print("Possible leak detected (definitely lost != 0)")
        print(stderr_str)
        return False
        
    if "All heap blocks were freed" in stderr_str:
        pass
    elif "indirectly lost: 0 bytes in 0 blocks" not in stderr_str:
        print("Possible leak detected (indirectly lost != 0)")
        print(stderr_str)
        return False

    print(f"{sig_name} (Upload) test passed.")
    return True

def test_signal_cgi(sig, sig_name):
    print(f"Testing {sig_name} (CGI)...")
    
    cmd = [
        "valgrind",
        "--leak-check=full",
        "--show-leak-kinds=all",
        "--track-fds=yes",
        "--error-exitcode=100",
        "./webserv",
        "sample/08_signal_test/webserv.conf"
    ]
    
    proc = subprocess.Popen(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        cwd=os.getcwd(),
        preexec_fn=os.setsid
    )
    
    if not wait_for_port(18099):
        print("Server failed to start")
        proc.kill()
        stdout, stderr = proc.communicate()
        print("STDOUT:", stdout.decode())
        print("STDERR:", stderr.decode())
        return False

    print(f"Server started (PID {proc.pid}). requesting slow CGI...")

    # Request slow CGI
    try:
        sock = socket.create_connection(("127.0.0.1", 18099), timeout=10)
        sock.sendall(b"GET /cgi/sleep.py HTTP/1.1\r\nHost: localhost\r\n\r\n")
        time.sleep(1) # Wait for CGI to start
    except Exception as e:
        print(f"Failed to connect/send: {e}")
        proc.kill()
        return False

    print(f"Sending {sig_name}...")
    os.kill(proc.pid, sig)
    
    try:
        stdout, stderr = proc.communicate(timeout=5)
    except subprocess.TimeoutExpired:
        print("Server failed to stop in time!")
        proc.kill()
        stdout, stderr = proc.communicate()
        print(stderr.decode())
        sock.close()
        return False

    sock.close()
    
    rc = proc.returncode
    print(f"Server exited with {rc}")
    
    stderr_str = stderr.decode()
    
    if rc == 100:
        print("Valgrind reported errors!")
        print(stderr_str)
        return False
        
    if "Open file descriptor" in stderr_str:
         if "socket" in stderr_str:
             print("Open socket descriptors detected!")
             print(stderr_str)
             return False

    if "All heap blocks were freed" in stderr_str:
        pass
    elif "definitely lost: 0 bytes in 0 blocks" not in stderr_str:
        print("Possible leak detected (definitely lost != 0)")
        print(stderr_str)
        return False
        
    if "All heap blocks were freed" in stderr_str:
        pass
    elif "indirectly lost: 0 bytes in 0 blocks" not in stderr_str:
        print("Possible leak detected (indirectly lost != 0)")
        print(stderr_str)
        return False

    print(f"{sig_name} (CGI) test passed.")
    return True

def main():
    if not os.path.exists("./webserv"):
        print("webserv executable not found")
        sys.exit(1)

    if not test_signal(signal.SIGINT, "SIGINT"):
        sys.exit(1)
        
    time.sleep(1)
    
    if not test_signal(signal.SIGTERM, "SIGTERM"):
        sys.exit(1)

    time.sleep(1)

    if not test_signal_busy(signal.SIGINT, "SIGINT"):
        sys.exit(1)

    time.sleep(1)

    if not test_signal_busy(signal.SIGTERM, "SIGTERM"):
        sys.exit(1)
        
    time.sleep(1)

    if not test_signal_upload(signal.SIGINT, "SIGINT"):
        sys.exit(1)

    time.sleep(1)
    
    if not test_signal_upload(signal.SIGTERM, "SIGTERM"):
        sys.exit(1)
        
    time.sleep(1)

    if not test_signal_cgi(signal.SIGINT, "SIGINT"):
        sys.exit(1)

    time.sleep(1)
    
    if not test_signal_cgi(signal.SIGTERM, "SIGTERM"):
        sys.exit(1)

    print("All signal tests passed.")

if __name__ == "__main__":
    main()
