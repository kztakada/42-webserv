#!/usr/bin/env python3
import subprocess
import time
import os
import signal
import sys
import socket


def find_single_child_pid(parent_pid, timeout=5):
    """Find the first child PID of parent_pid (Linux /proc).

    We run webserv under valgrind, so proc.pid is valgrind's PID.
    To test webserv's signal handling/cleanup, we must signal the child.
    """
    children_path = f"/proc/{parent_pid}/task/{parent_pid}/children"
    start = time.time()
    last_err = None
    while time.time() - start < timeout:
        try:
            with open(children_path, "r") as f:
                content = f.read().strip()
            if content:
                # content is a whitespace-separated list of pids
                pids = [int(x) for x in content.split() if x.strip()]
                if len(pids) >= 1:
                    return pids[0]
        except Exception as e:
            last_err = e
        time.sleep(0.05)
    raise RuntimeError(f"Failed to find child PID for {parent_pid}: {last_err}")


def read_valgrind_log(path="valgrind.log"):
    if os.path.exists(path):
        with open(path, "r") as f:
            return f.read()
    return ""


def has_non_inherited_socket_fds(valgrind_output):
    lines = valgrind_output.splitlines()
    for i, line in enumerate(lines):
        if "Open" in line and "socket" in line:
            inherited = False
            # valgrind prints "<inherited from parent>" on a subsequent line
            for j in range(i + 1, min(i + 6, len(lines))):
                if "<inherited from parent>" in lines[j]:
                    inherited = True
                    break
            if not inherited:
                return True
    return False


def list_non_inherited_open_fds(valgrind_output):
    """Return human-readable lines describing open FDs not inherited.

    We intentionally ignore entries marked as inherited from the parent process
    (e.g. VS Code terminal PTYs, valgrind log FD passed down).
    """
    findings = []
    lines = valgrind_output.splitlines()
    for i, line in enumerate(lines):
        if not line.strip().startswith("Open "):
            continue

        # Consider both "Open file descriptor" and various socket types.
        if ("Open file descriptor" not in line) and ("socket" not in line):
            continue

        inherited = False
        for j in range(i + 1, min(i + 6, len(lines))):
            if "<inherited from parent>" in lines[j]:
                inherited = True
                break
        if inherited:
            continue

        findings.append(line.strip())

    return findings

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

    if os.path.exists("valgrind.log"):
        os.remove("valgrind.log")
    
    # Valgrind command
    # We use --error-exitcode=100 to detect memory errors
    cmd = [
        "valgrind",
        "--leak-check=full",
        "--show-leak-kinds=all",
        "--track-fds=yes",
        "--error-exitcode=100",
        "--log-file=valgrind.log",
        "--verbose",
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

    target_pid = proc.pid
    try:
        # Some setups run the client within the same PID as valgrind.
        # If a child exists, we prefer signaling the child.
        target_pid = find_single_child_pid(proc.pid, timeout=0.5)
        print(
            f"Server started (valgrind PID {proc.pid}, webserv PID {target_pid}). Sending {sig_name}..."
        )
    except Exception:
        print(f"Server started (PID {proc.pid}). Sending {sig_name}...")
    
    # Send signal
    os.kill(target_pid, sig)
    time.sleep(1)
    
    try:
        stdout, stderr = proc.communicate(timeout=5)
    except subprocess.TimeoutExpired:
        print("Server failed to stop in time!")
        proc.kill()
        stdout, stderr = proc.communicate()
        print(stderr.decode())
        return False

    rc = proc.returncode
    print(f"Valgrind exited with {rc}")
    
    stderr_str = read_valgrind_log("valgrind.log")

    if rc != 0 and rc != 100:
        print("Non-zero exit code detected")
        print(stderr_str)
        return False
    
    # Check for valgrind errors
    if rc == 100:
        print("Valgrind reported errors!")
        print(stderr_str)
        return False
        
    if has_non_inherited_socket_fds(stderr_str):
        print("Open socket descriptors detected (not inherited)!")
        print(stderr_str)
        return False

    non_inherited_fds = list_non_inherited_open_fds(stderr_str)
    if non_inherited_fds:
        print("Open file descriptors detected (not inherited)!")
        print("\n".join(non_inherited_fds))
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

    if os.path.exists("valgrind.log"):
        os.remove("valgrind.log")
    
    cmd = [
        "valgrind",
        "--leak-check=full",
        "--show-leak-kinds=all",
        "--track-fds=yes",
        "--error-exitcode=100",
        "--log-file=valgrind.log",
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

    target_pid = proc.pid
    try:
        target_pid = find_single_child_pid(proc.pid, timeout=0.5)
        print(
            f"Server started (valgrind PID {proc.pid}, webserv PID {target_pid}). sending partial request..."
        )
    except Exception:
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
    os.kill(target_pid, sig)
    
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
    
    stderr_str = read_valgrind_log("valgrind.log")

    if rc != 0 and rc != 100:
        print("Non-zero exit code detected")
        print(stderr_str)
        return False
    
    if rc == 100:
        print("Valgrind reported errors!")
        print(stderr_str)
        return False

    if has_non_inherited_socket_fds(stderr_str):
        print("Open socket descriptors detected (not inherited)!")
        print(stderr_str)
        return False

    non_inherited_fds = list_non_inherited_open_fds(stderr_str)
    if non_inherited_fds:
        print("Open file descriptors detected (not inherited)!")
        print("\n".join(non_inherited_fds))
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

    if os.path.exists("valgrind.log"):
        os.remove("valgrind.log")
    
    cmd = [
        "valgrind",
        "--leak-check=full",
        "--show-leak-kinds=all",
        "--track-fds=yes",
        "--error-exitcode=100",
        "--log-file=valgrind.log",
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

    target_pid = proc.pid
    try:
        target_pid = find_single_child_pid(proc.pid, timeout=0.5)
        print(
            f"Server started (valgrind PID {proc.pid}, webserv PID {target_pid}). sending large body..."
        )
    except Exception:
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
    os.kill(target_pid, sig)
    
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
    
    stderr_str = read_valgrind_log("valgrind.log")

    if rc != 0 and rc != 100:
        print("Non-zero exit code detected")
        print(stderr_str)
        return False
    
    if rc == 100:
        print("Valgrind reported errors!")
        print(stderr_str)
        return False
        
    if has_non_inherited_socket_fds(stderr_str):
        print("Open socket descriptors detected (not inherited)!")
        print(stderr_str)
        return False

    non_inherited_fds = list_non_inherited_open_fds(stderr_str)
    if non_inherited_fds:
        print("Open file descriptors detected (not inherited)!")
        print("\n".join(non_inherited_fds))
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

    if os.path.exists("valgrind.log"):
        os.remove("valgrind.log")
    
    cmd = [
        "valgrind",
        "--leak-check=full",
        "--show-leak-kinds=all",
        "--track-fds=yes",
        "--error-exitcode=100",
        "--log-file=valgrind.log",
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

    target_pid = proc.pid
    try:
        target_pid = find_single_child_pid(proc.pid, timeout=0.5)
        print(
            f"Server started (valgrind PID {proc.pid}, webserv PID {target_pid}). requesting slow CGI..."
        )
    except Exception:
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
    os.kill(target_pid, sig)
    
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
    
    stderr_str = read_valgrind_log("valgrind.log")

    if rc != 0 and rc != 100:
        print("Non-zero exit code detected")
        print(stderr_str)
        return False
    
    if rc == 100:
        print("Valgrind reported errors!")
        print(stderr_str)
        return False
        
    if has_non_inherited_socket_fds(stderr_str):
        print("Open socket descriptors detected (not inherited)!")
        print(stderr_str)
        return False

    non_inherited_fds = list_non_inherited_open_fds(stderr_str)
    if non_inherited_fds:
        print("Open file descriptors detected (not inherited)!")
        print("\n".join(non_inherited_fds))
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
