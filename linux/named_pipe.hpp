#pragma once
namespace Linux{
    struct pipe_fd{
        int input;
        int output;
    };
    auto create_pipe() {
        int fd[2];
        if (-1 == ::pipe(fd)) {
            throw std::runtime_error{strerror(errno)};
        }
        return pipe_fd{fd[0],fd[1]};
    }
    auto create_process(std::filesystem::path path, int out) {
        if (fork()) {
        }
        else {
            dup2(out, STDOUT_FILENO);
            execl(path.c_str(), path.c_str(), NULL);
        }
    }
    class process{
    public:
        process(std::filesystem::path path, int out) {
            if (fork()) {
            }
            else {
                dup2(out, STDOUT_FILENO);
                execl(path.c_str(), path.c_str(), NULL);
            }
        }
    };
}
using namespace Linux;
