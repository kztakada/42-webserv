// Auto-generated E2E test (directory-style like sample/)
//
// This program:
// - starts ./webserv with ./webserv.conf
// - sends a raw HTTP request
// - checks the response status code

#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>

extern char** environ;

static void sleepMillis(int millis)
{
    if (millis <= 0)
    {
        return;
    }
    struct timeval tv;
    tv.tv_sec = millis / 1000;
    tv.tv_usec = (millis % 1000) * 1000;
    ::select(0, NULL, NULL, NULL, &tv);
}

static bool parseListenPort(const std::string& conf_path, int& out_port)
{
    std::ifstream ifs(conf_path.c_str());
    if (!ifs)
    {
        return false;
    }
    std::string line;
    while (std::getline(ifs, line))
    {
        if (line.find("listen") == std::string::npos)
        {
            continue;
        }
        std::string::size_type colon = line.find(':');
        if (colon == std::string::npos)
        {
            continue;
        }
        std::string::size_type semi = line.find(';', colon);
        if (semi == std::string::npos)
        {
            continue;
        }
        std::string port_str = line.substr(colon + 1, semi - (colon + 1));
        while (!port_str.empty() && (port_str[0] == ' ' || port_str[0] == '\t'))
        {
            port_str.erase(0, 1);
        }
        while (!port_str.empty() && (port_str[port_str.size() - 1] == ' ' ||
                                        port_str[port_str.size() - 1] == '\t'))
        {
            port_str.erase(port_str.size() - 1, 1);
        }
        int port = std::atoi(port_str.c_str());
        if (port <= 0 || port > 65535)
        {
            continue;
        }
        out_port = port;
        return true;
    }
    return false;
}

static pid_t startWebserv(const std::string& repo_root_rel,
    const std::string& webserv_rel, const std::string& conf_abs)
{
    pid_t pid = ::fork();
    if (pid < 0)
    {
        return -1;
    }
    if (pid == 0)
    {
        if (::chdir(repo_root_rel.c_str()) != 0)
        {
            std::perror("chdir(repo_root)");
            std::exit(127);
        }
        char* const argv_exec[] = {
            const_cast<char*>(webserv_rel.c_str()),
            const_cast<char*>(conf_abs.c_str()),
            NULL,
        };
        ::execve(argv_exec[0], argv_exec, environ);
        std::perror("execve(webserv)");
        std::exit(127);
    }
    return pid;
}

static int connectWithRetry(int port, int timeout_ms)
{
    const int kStepMs = 50;
    int elapsed = 0;
    while (elapsed <= timeout_ms)
    {
        int fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0)
        {
            return -1;
        }
        struct sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(static_cast<unsigned short>(port));
        addr.sin_addr.s_addr = htonl(0x7f000001u);
        if (::connect(fd, reinterpret_cast<struct sockaddr*>(&addr),
                sizeof(addr)) == 0)
        {
            return fd;
        }
        ::close(fd);
        sleepMillis(kStepMs);
        elapsed += kStepMs;
    }
    return -1;
}

static bool sendAll(int fd, const std::string& data)
{
    size_t sent = 0;
    while (sent < data.size())
    {
        ssize_t n = ::send(fd, data.data() + sent, data.size() - sent, 0);
        if (n <= 0)
        {
            return false;
        }
        sent += static_cast<size_t>(n);
    }
    return true;
}

static bool recvWithTimeout(int fd, std::string& inout, int timeout_ms)
{
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    int rc = ::select(fd + 1, &rfds, NULL, NULL, &tv);
    if (rc <= 0)
    {
        return false;
    }
    char buf[4096];
    ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
    if (n <= 0)
    {
        return false;
    }
    inout.append(buf, buf + n);
    return true;
}

static long long nowMillis()
{
    struct timeval tv;
    if (::gettimeofday(&tv, NULL) != 0)
    {
        return 0;
    }
    return static_cast<long long>(tv.tv_sec) * 1000LL +
           static_cast<long long>(tv.tv_usec) / 1000LL;
}

static bool recvWithDeadline(int fd, std::string& inout, long long deadline_ms)
{
    while (true)
    {
        long long remain = deadline_ms - nowMillis();
        if (remain <= 0)
        {
            return false;
        }
        int slice_ms = 200;
        if (remain < slice_ms)
        {
            slice_ms = static_cast<int>(remain);
            if (slice_ms <= 0)
            {
                return false;
            }
        }
        if (recvWithTimeout(fd, inout, slice_ms))
        {
            return true;
        }
    }
}

static char toLowerAscii(char c);

static bool parseContentLength(const std::string& header, size_t& out_len)
{
    const std::string key = "content-length:";
    std::string lower;
    lower.reserve(header.size());
    for (std::string::size_type i = 0; i < header.size(); ++i)
    {
        lower.push_back(toLowerAscii(header[i]));
    }
    std::string::size_type p = lower.find(key);
    if (p == std::string::npos)
    {
        return false;
    }
    p += key.size();
    while (p < header.size() && (header[p] == ' ' || header[p] == '\t'))
    {
        ++p;
    }
    std::string::size_type eol = header.find("\r\n", p);
    if (eol == std::string::npos)
    {
        eol = header.find('\n', p);
        if (eol == std::string::npos)
        {
            return false;
        }
    }
    std::string num = header.substr(p, eol - p);
    while (!num.empty() &&
           (num[num.size() - 1] == ' ' || num[num.size() - 1] == '\t'))
    {
        num.erase(num.size() - 1, 1);
    }
    if (num.empty())
    {
        return false;
    }
    char* end = NULL;
    unsigned long v = std::strtoul(num.c_str(), &end, 10);
    if (end == num.c_str() || (end != NULL && *end != '\0'))
    {
        return false;
    }
    out_len = static_cast<size_t>(v);
    return true;
}

static char toLowerAscii(char c)
{
    if (c >= 'A' && c <= 'Z')
    {
        return static_cast<char>(c - 'A' + 'a');
    }
    return c;
}

static bool headerHasChunkedTransferEncoding(const std::string& header)
{
    // Case-insensitive search for "Transfer-Encoding" and "chunked".
    std::string lower;
    lower.reserve(header.size());
    for (std::string::size_type i = 0; i < header.size(); ++i)
    {
        lower.push_back(toLowerAscii(header[i]));
    }

    std::string::size_type p = 0;
    while (true)
    {
        p = lower.find("transfer-encoding:", p);
        if (p == std::string::npos)
        {
            return false;
        }
        std::string::size_type eol = lower.find("\r\n", p);
        if (eol == std::string::npos)
        {
            return false;
        }
        const std::string line = lower.substr(p, eol - p);
        if (line.find("chunked") != std::string::npos)
        {
            return true;
        }
        p = eol + 2;
    }
}

static bool recvOneHttpResponse(
    int fd, std::string& inout_buf, std::string& out_response)
{
    out_response.clear();
    const long long deadline_ms = nowMillis() + 5000;

    // Read until header end.
    while (inout_buf.find("\r\n\r\n") == std::string::npos)
    {
        if (inout_buf.size() > 256 * 1024)
        {
            return false;
        }
        if (!recvWithDeadline(fd, inout_buf, deadline_ms))
        {
            return false;
        }
    }

    const std::string::size_type header_end = inout_buf.find("\r\n\r\n");
    const std::string header = inout_buf.substr(0, header_end + 4);
    const std::string::size_type body_start = header_end + 4;

    // Determine framing.
    std::string::size_type message_end = body_start;
    if (headerHasChunkedTransferEncoding(header))
    {
        std::string::size_type cur = body_start;
        while (true)
        {
            // Read chunk-size line.
            while (true)
            {
                std::string::size_type line_end = inout_buf.find("\r\n", cur);
                if (line_end != std::string::npos)
                {
                    std::string line = inout_buf.substr(cur, line_end - cur);
                    std::string::size_type semi = line.find(';');
                    if (semi != std::string::npos)
                    {
                        line = line.substr(0, semi);
                    }
                    while (!line.empty() && (line[0] == ' ' || line[0] == '\t'))
                    {
                        line.erase(0, 1);
                    }
                    while (!line.empty() && (line[line.size() - 1] == ' ' ||
                                                line[line.size() - 1] == '\t'))
                    {
                        line.erase(line.size() - 1, 1);
                    }

                    char* end = NULL;
                    unsigned long chunk_size =
                        std::strtoul(line.c_str(), &end, 16);
                    if (end == line.c_str() || (end != NULL && *end != '\0'))
                    {
                        return false;
                    }

                    cur = line_end + 2;
                    const std::string::size_type need =
                        cur + static_cast<std::string::size_type>(chunk_size) +
                        2;
                    while (inout_buf.size() < need)
                    {
                        if (!recvWithDeadline(fd, inout_buf, deadline_ms))
                        {
                            return false;
                        }
                    }
                    cur = need;

                    if (chunk_size == 0)
                    {
                        // Trailers end with CRLFCRLF.
                        while (inout_buf.find("\r\n\r\n", cur) ==
                               std::string::npos)
                        {
                            if (!recvWithDeadline(fd, inout_buf, deadline_ms))
                            {
                                return false;
                            }
                        }
                        message_end = inout_buf.find("\r\n\r\n", cur) + 4;
                        break;
                    }
                    break;
                }
                if (!recvWithDeadline(fd, inout_buf, deadline_ms))
                {
                    return false;
                }
                if (inout_buf.size() > 256 * 1024)
                {
                    return false;
                }
            }
            if (message_end != body_start)
            {
                break;
            }
        }
    }
    else
    {
        size_t cl = 0;
        if (parseContentLength(header, cl))
        {
            const std::string::size_type need =
                body_start + static_cast<std::string::size_type>(cl);
            while (inout_buf.size() < need)
            {
                if (!recvWithDeadline(fd, inout_buf, deadline_ms))
                {
                    return false;
                }
            }
            message_end = need;
        }
        else
        {
            // No known body framing; treat as header-only.
            message_end = body_start;
        }
    }

    if (inout_buf.size() < message_end)
    {
        return false;
    }
    out_response = inout_buf.substr(0, message_end);
    inout_buf.erase(0, message_end);
    return true;
}

static int parseStatusCode(const std::string& response)
{
    std::string::size_type line_end = response.find("\r\n");
    if (line_end == std::string::npos)
    {
        line_end = response.find('\n');
        if (line_end == std::string::npos)
        {
            return -1;
        }
    }
    std::string line = response.substr(0, line_end);
    std::string::size_type sp = line.find(' ');
    if (sp == std::string::npos)
    {
        return -1;
    }
    if (line.size() < sp + 4)
    {
        return -1;
    }
    int code = std::atoi(line.substr(sp + 1, 3).c_str());
    return code;
}

int main()
{
    const std::string conf_rel = "webserv.conf";
    const std::string repo_root_rel = "../../..";
    const std::string webserv_rel = "./webserv";
    const int expected_status = 200;
    const char* test_name = "connection_keep_alive";

    char cwd_buf[4096];
    if (::getcwd(cwd_buf, sizeof(cwd_buf)) == NULL)
    {
        std::cerr << "FAIL: getcwd: " << std::strerror(errno) << "\n";
        return 1;
    }
    std::string conf_abs = std::string(cwd_buf) + "/" + conf_rel;

    int port = -1;
    if (!parseListenPort(conf_abs, port))
    {
        std::cerr << "FAIL: cannot parse listen port from " << conf_abs << "\n";
        return 1;
    }

    pid_t server_pid = startWebserv(repo_root_rel, webserv_rel, conf_abs);
    if (server_pid < 0)
    {
        std::cerr << "FAIL: fork() failed\n";
        return 1;
    }

    int client_fd = connectWithRetry(port, 2000);
    if (client_fd < 0)
    {
        std::cerr << "FAIL: cannot connect to 127.0.0.1:" << port << "\n";
        ::kill(server_pid, SIGINT);
        ::waitpid(server_pid, NULL, 0);
        return 1;
    }

    const std::string request1 = std::string(
        "GET / HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive\r\n\r\n");
    if (!sendAll(client_fd, request1))
    {
        std::cerr << "FAIL: send() failed\n";
        ::close(client_fd);
        ::kill(server_pid, SIGINT);
        ::waitpid(server_pid, NULL, 0);
        return 1;
    }

    std::string recv_buf;
    std::string response1;
    if (!recvOneHttpResponse(client_fd, recv_buf, response1))
    {
        std::cerr << "FAIL: recv() timeout while waiting first response\n";
        ::close(client_fd);
        ::kill(server_pid, SIGINT);
        ::waitpid(server_pid, NULL, 0);
        return 1;
    }

    int status = parseStatusCode(response1);
    if (status != expected_status)
    {
        std::cerr << "FAIL: " << test_name << " expected " << expected_status
                  << " but got " << status << "\n";
        std::cerr << "--- response (head) ---\n";
        std::string::size_type p = response1.find("\r\n\r\n");
        if (p != std::string::npos)
        {
            std::cerr << response1.substr(0, p + 4) << "\n";
        }
        else
        {
            std::cerr << response1 << "\n";
        }
        ::kill(server_pid, SIGINT);
        ::waitpid(server_pid, NULL, 0);
        return 1;
    }

    // Send second request to confirm the connection stays usable, and ask
    // server to close.
    const std::string request2 = std::string(
        "GET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n");
    if (!sendAll(client_fd, request2))
    {
        std::cerr << "FAIL: send() failed (second request)\n";
        ::close(client_fd);
        ::kill(server_pid, SIGINT);
        ::waitpid(server_pid, NULL, 0);
        return 1;
    }

    std::string response2;
    if (!recvOneHttpResponse(client_fd, recv_buf, response2))
    {
        std::cerr << "FAIL: recv() timeout while waiting second response\n";
        ::close(client_fd);
        ::kill(server_pid, SIGINT);
        ::waitpid(server_pid, NULL, 0);
        return 1;
    }
    int status2 = parseStatusCode(response2);
    if (status2 != expected_status)
    {
        std::cerr << "FAIL: " << test_name << " expected " << expected_status
                  << " but got " << status2 << " (second response)\n";
        std::cerr << "--- response (head) ---\n";
        std::string::size_type p = response2.find("\r\n\r\n");
        if (p != std::string::npos)
        {
            std::cerr << response2.substr(0, p + 4) << "\n";
        }
        else
        {
            std::cerr << response2 << "\n";
        }
        ::close(client_fd);
        ::kill(server_pid, SIGINT);
        ::waitpid(server_pid, NULL, 0);
        return 1;
    }
    ::close(client_fd);

    ::kill(server_pid, SIGINT);
    ::waitpid(server_pid, NULL, 0);
    std::cout << "PASS: " << test_name << " (status=" << status << ")\n";
    return 0;
}
