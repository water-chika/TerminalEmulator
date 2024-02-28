#include "Windows.h"

#include <strstream>

namespace windows {
    void throw_last_error() {
        auto err = GetLastError();
        std::stringstream err_stream{};
        err_stream << "Error code: " << err;
        throw std::runtime_error{ err_stream.str()};
    }
    void close_handle(HANDLE handle) {
        if (0 == ::CloseHandle(handle)) {
            throw_last_error();
        }
    }
    void disconnect_named_pipe(HANDLE handle) {
        if (0 == ::DisconnectNamedPipe(handle)) {
            throw_last_error();
        }
    }
    HANDLE create_named_pipe(const std::string path) {
        SECURITY_ATTRIBUTES secu_attr{};
        //secu_attr.bInheritHandle = TRUE;
        auto handle = CreateNamedPipe(path.c_str(),
            PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE,
            1,
            256,
            256,
            0,
            &secu_attr
        );
        if (handle == INVALID_HANDLE_VALUE) {
            throw_last_error();
        }
        return handle;
    }
    HANDLE create_file(const std::string path) {
        SECURITY_ATTRIBUTES secu_attr{};
        secu_attr.bInheritHandle = TRUE;
        auto handle = CreateFile(
            path.c_str(),
            GENERIC_WRITE,
            0,
            &secu_attr,
            OPEN_EXISTING,
            0,
            NULL
        );
        if (handle == INVALID_HANDLE_VALUE) {
            throw_last_error();
        }
        return handle;
    }

    class named_pipe {
    public:
        named_pipe(const std::string path) {
            SECURITY_ATTRIBUTES secu_attr{};
            //secu_attr.bInheritHandle = TRUE;
            m_handle = CreateNamedPipe(path.c_str(),
                PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
                PIPE_TYPE_BYTE | PIPE_READMODE_BYTE,
                1,
                256,
                256,
                0,
                &secu_attr
            );
            if (m_handle == INVALID_HANDLE_VALUE) {
                throw_last_error();
            }
            assert(m_handle != INVALID_HANDLE_VALUE);
        }
        ~named_pipe() noexcept(false) {
            FlushFileBuffers(m_handle);
            disconnect_named_pipe(m_handle);
            close_handle(m_handle);
        }

        operator HANDLE() {
            return m_handle;
        }
    private:
        HANDLE m_handle;
    };

    class opened_named_pipe {
    public:
        opened_named_pipe(const std::string path) {
            m_handle = create_file(path);
        }
        ~opened_named_pipe() {
            close_handle(m_handle);
        }

        auto native_handle() {
            return m_handle;
        }
    private:
        HANDLE m_handle;
    };

    class process {
    public:
        process(std::filesystem::path path, opened_named_pipe& out) {
            STARTUPINFOW si;
            PROCESS_INFORMATION pi;
            ZeroMemory(&si, sizeof(si));
            si.cb = sizeof(si);
            ZeroMemory(&pi, sizeof(pi));
            si.dwFlags |= STARTF_USESTDHANDLES;
            si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
            si.hStdOutput = out.native_handle();
            si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
            // Start the child process. 
            if (!::CreateProcessW(path.c_str(),   // No module name (use command line)
                NULL,        // Command line
                NULL,           // Process handle not inheritable
                NULL,           // Thread handle not inheritable
                TRUE,          // Set handle inheritance to FALSE
                0,              // No creation flags
                NULL,           // Use parent's environment block
                NULL,           // Use parent's starting directory 
                &si,            // Pointer to STARTUPINFO structure
                &pi)           // Pointer to PROCESS_INFORMATION structure
                )
            {
                printf("CreateProcess failed (%d).\n", GetLastError());
                return;
            }
            m_process = pi.hProcess;
            m_thread = pi.hThread;
        }
        ~process() {
            close_handle(m_thread);
            close_handle(m_process);
        }
    private:
        HANDLE m_process;
        HANDLE m_thread;
    };
    struct pipe_handles{
        HANDLE input;
        HANDLE output;
    };
    auto create_pipe() {
        SECURITY_ATTRIBUTES secu_attr{};
        secu_attr.bInheritHandle = TRUE;
        std::stringstream pipe_name_stream;
        pipe_name_stream << "\\\\.\\pipe\\terminal_emulator-" << std::chrono::steady_clock::now().time_since_epoch();
        std::string pipe_name = std::move(pipe_name_stream).str();
        HANDLE input = windows::create_named_pipe(pipe_name);
        HANDLE output = create_file(pipe_name);
        return pipe_handles{input, output};
    }
    auto create_process(std::filesystem::path path, opened_named_pipe& out) {
        return windows::process{path, out};
    }
}
using namespace windows;
