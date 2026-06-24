#include <pty.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <poll.h>

#include <iostream>
#include <exception>
#include <vector>
#include <array>

int main(int argc, char** argv) {
    try {
        if (argc < 2) {
            throw std::runtime_error("wrong arguments count");
        }
        int ret = socket(PF_INET, SOCK_STREAM, 0);
        if (ret < 0) {
            throw std::runtime_error("socket fail");
        }
        auto socket = ret;

        auto hostname = argv[1];
        auto port = (uint16_t)strtol(argv[2], nullptr, 10);

        struct hostent *hostinfo;
        sockaddr_in name{};
        name.sin_family = AF_INET;
        name.sin_port = htons(port);
        hostinfo = gethostbyname(hostname);
        if (hostinfo == NULL) {
            throw std::runtime_error("Unknown host");
        }
        name.sin_addr = *(in_addr*)hostinfo->h_addr;

        if (0 > connect(socket, (sockaddr*)&name, sizeof(name))) {
            throw std::runtime_error("connect fail");
        }

        auto buffer = std::vector<char>(256);
        int sock = socket;
        int in = STDIN_FILENO;

        auto fds = std::array<pollfd,2>{
            pollfd{.fd = sock, .events = POLLIN},
            pollfd{.fd = in, .events = POLLIN},
        };
        while (true) {
            int ret = poll(fds.data(), fds.size(), -1);
            if (ret > 0) {
                if (fds[0].revents & POLLIN) {
                    auto ret = recv(socket, buffer.data(), buffer.size(), 0);
                    write(STDOUT_FILENO, buffer.data(), ret);
                }
                if (fds[1].revents & POLLIN) {
                    ret = read(STDIN_FILENO, buffer.data(), buffer.size());
                    send(socket, buffer.data(), ret, 0);
                }
            }
        }

    }
    catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
    return 0;
}
