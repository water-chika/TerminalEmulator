#include <iostream>
#include <vector>
#include <cstdint>
#include <array>

#include <pty.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <poll.h>

template<uint16_t PORT, typename T>
class set_static_port : public T{
public:
    uint16_t get_port() { return PORT; }
};

template<typename T>
class add_socket_bind : public T{
    using parent = T;
public:
    add_socket_bind() : T{} {
        m_socket = make_socket(parent::get_port());
        if (listen(m_socket, 1) < 0) {
            throw std::runtime_error("listen failed");
        }
        sockaddr_in clientname;
        socklen_t size;
        m_client_socket = accept(m_socket, (struct sockaddr*)&clientname, &size);
    }
    ~add_socket_bind() {
        close(m_socket);
    }
    int get_socket() {
        return m_socket;
    }
    int get_client_socket() {
        return m_client_socket;
    }
    static int make_socket(uint16_t port) {
        int sock = -1;

        sock = socket(PF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            throw std::runtime_error("socket create fail");
        }

        sockaddr_in name{};
        name.sin_family = AF_INET;
        name.sin_port = htons(port);
        name.sin_addr.s_addr = htonl(INADDR_ANY);

        if (bind(sock, (sockaddr*)&name, sizeof(name)) < 0) {
            throw std::runtime_error("bind fail");
        }

        return sock;
    }
private:
    int m_socket;
    int m_client_socket;
};

template<typename T>
class add_pty_shell : public T {
public:
    add_pty_shell() {
        int master{};
        char name[256];
        termios term{};
            term.c_iflag,
            term.c_oflag,
            term.c_cflag = CLOCAL | CREAD | CS8,
            term.c_lflag,
            term.c_cc;
        winsize win{
            96, 102
        };
        int ret = forkpty(&master, name, &term, &win);
        if (ret == -1) {
            throw std::runtime_error("forkpty failed");
        }
        if (ret == 0) {
            execl("/bin/sh", "/bin/sh", NULL);
            // child
        }
        int child_pid = ret;
        std::cout << "child pid: " << child_pid << std::endl;
        std::cout << "pseudo-terminal name: " << name << std::endl;
        m_master = master;
        m_child_pid = child_pid;
    }
    ~add_pty_shell() {
        try{
            int status{};
            int ret = waitpid(m_child_pid, &status, 0);
            if (ret == -1) {
                throw std::runtime_error("waitpid failed");
            }
        }
        catch (std::exception& e) {
            std::cerr << e.what();
        }
    }
    int get_pty_master() {
        return m_master;
    }
private:
    int m_master;
    int m_child_pid;
};


template<typename T>
class add_event_loop : public T {
    using parent = T;
public:
    add_event_loop() {

        auto sock = parent::get_client_socket();
        auto pty_master = parent::get_pty_master();
        auto fds = std::array<pollfd,2>{
            pollfd{.fd = sock, .events = POLLIN},
            pollfd{.fd = pty_master, .events = POLLIN},
        };
        auto buffer = std::vector<char>(256);
        while (true) {
            int ret = poll(fds.data(), fds.size(), -1);
            if (ret > 0) {
                if (fds[0].revents & POLLIN) {
                    int ret = recv(sock, buffer.data(), buffer.size(), 0);
                    write(STDOUT_FILENO, "socket get: ", 12);
                    write(STDOUT_FILENO, buffer.data(), ret);
                    write(STDOUT_FILENO, "\n", 1);
                    write(pty_master, buffer.data(), ret);
                }
                if (fds[1].revents & POLLIN) {
                    int ret = read(pty_master, buffer.data(), buffer.size());
                    send(sock, buffer.data(), ret, 0);
                }
            }
        }
    }
};

struct empty_struct{};

using server =
            add_event_loop<
            add_pty_shell<
            add_socket_bind<
            set_static_port<10022,
            empty_struct
>>>>;

int main(void) {
    try {
        server test_server{}; 
    }
    catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
    return 0;
}
