#include <pty.h>
#include <unistd.h>
#include <sys/wait.h>

#include <iostream>
#include <vector>
#include <string>

int main(void) {
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
        std::cerr << "forkpty failed" << std::endl;
        return -1;
    }
    if (ret == 0) {
        execl("/bin/sh", "/bin/sh", NULL);
        // child
    }
    int child_pid = ret;
    std::cout << "child pid: " << child_pid << std::endl;
    std::cout << "pseudo-terminal name: " << name << std::endl;

    auto buffer = std::vector<char>(64);

    auto input = std::string{"ls\n"};
    ret = write(master, input.data(), input.size());
    if (ret == -1) {
        std::cerr << "write failed" << std::endl;
        return -1;
    }
    if (ret != input.size()) {
        std::cerr << "wirte not full" << std::endl;
    }

    sleep(1);
    ret = read(master, buffer.data(), buffer.size());
    if (ret == -1) {
        std::cerr << "read failed" << std::endl;
        return -1;
    }
    std::cout << "pts output:";
    for (int i = 0; i < ret; i++) {
        std::cout << buffer[i];
    }
    std::cout << "pts output end" << std::endl;

    int status{};
    ret = waitpid(child_pid, &status, 0);
    if (ret == -1) {
        std::cerr << "waitpid failed" << std::endl;
        return -1;
    }

    return 0;
}
