// Auto-generated E2E test (directory-style like sample/)
//
// This program:
// - starts ./webserv with ./webserv.conf
// - sends a raw HTTP request
// - checks the response status code

#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>

#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

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
		while (!port_str.empty() && (port_str[port_str.size() - 1] == ' ' || port_str[port_str.size() - 1] == '\t'))
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

static pid_t startWebserv(const std::string& repo_root_rel, const std::string& webserv_rel, const std::string& conf_abs)
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
		if (::connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == 0)
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

static std::string recvSome(int fd)
{
	std::string out;
	char buf[4096];
	while (true)
	{
		ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
		if (n <= 0)
		{
			break;
		}
		out.append(buf, buf + n);
		if (out.size() > 256 * 1024)
		{
			break;
		}
	}
	return out;
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
	const int expected_status = 400;
	const char* test_name = "status_header_date";

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

	const std::string request = std::string("GET / HTTP/1.1\r\nConnection: close\r\n\r\n");
	if (!sendAll(client_fd, request))
	{
		std::cerr << "FAIL: send() failed\n";
		::close(client_fd);
		::kill(server_pid, SIGINT);
		::waitpid(server_pid, NULL, 0);
		return 1;
	}

	std::string response = recvSome(client_fd);
	::close(client_fd);

	int status = parseStatusCode(response);
	if (status != expected_status)
	{
		std::cerr << "FAIL: " << test_name << " expected " << expected_status << " but got " << status << "\n";
		std::cerr << "--- response (head) ---\n";
		std::string::size_type p = response.find("\r\n\r\n");
		if (p != std::string::npos)
		{
			std::cerr << response.substr(0, p + 4) << "\n";
		}
		else
		{
			std::cerr << response << "\n";
		}
		::kill(server_pid, SIGINT);
		::waitpid(server_pid, NULL, 0);
		return 1;
	}

	::kill(server_pid, SIGINT);
	::waitpid(server_pid, NULL, 0);
	std::cout << "PASS: " << test_name << " (status=" << status << ")\n";
	return 0;
}
