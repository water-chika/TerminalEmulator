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
}
