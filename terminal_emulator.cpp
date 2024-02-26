#include "boost/asio.hpp"
#include "vulkan_render.hpp"
#include "font_loader.hpp"

#include <GLFW/glfw3.h>

#undef min
#include <set>
#include <map>
#include <memory>
#include <utility>
#include <functional>
#include <chrono>
#include <thread>
#include <strstream>

#include "multidimention_array.hpp"
#include "run_result.hpp"

bool contains(const auto&& range, const auto&& v) {
    return range.end() != range.find(v);
}

struct glfwContext
{
    glfwContext()
    {
        glfwInit();
        glfwSetErrorCallback(
            [](int error, const char* msg)
            {
                std::cerr << "glfw: " << "(" << error << ") " << msg << std::endl;
            }
        );
        assert(GLFW_TRUE == glfwVulkanSupported());
    }
    ~glfwContext() {
        glfwTerminate();
    }
};

class window_manager {
public:
    window_manager() : window{ create_window() } {
        window_map.emplace(window, this);
        glfwSetCharCallback(window, character_callback);
    }
    auto get_window() {
        return window;
    }
    void set_process_character_fun(auto&& fun) {
        process_character_fun = fun;
    }
    void process_character(uint32_t codepoint) {
        std::cout << codepoint;
        process_character_fun(codepoint);
    }
    static void character_callback(GLFWwindow* window, unsigned int codepoint) {
        auto manager = window_map[window];
        manager->process_character(codepoint);
    }
    run_result run() {
        glfwPollEvents();
        return glfwWindowShouldClose(window) ? run_result::eBreak : run_result::eContinue;
    }
private:
    static GLFWwindow* create_window() {
        uint32_t width = 1024, height = 1024;
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        return glfwCreateWindow(width, height, "Terminal Emulator", nullptr, nullptr);
    }
    glfwContext glfw_context;
    GLFWwindow* window;
    static std::map<GLFWwindow*, window_manager*> window_map;
    std::function<void(uint32_t)> process_character_fun;
};
std::map<GLFWwindow*, window_manager*> window_manager::window_map;

using namespace std::literals;
class terminal_emulator {
public:
    terminal_emulator() :
        m_window_manager{}, m_render{}, m_buffer{} {
        std::string str = "hello world! Wow, do you think this is a good start? ...............abcdefghijklmnopqrstuvwxyz";
        std::copy(str.begin(), str.end(), m_buffer.begin());
        GLFWwindow* window = m_window_manager.get_window();
        m_render.init([window](VkInstance instance) {
            VkSurfaceKHR surface;
            assert(VK_SUCCESS == glfwCreateWindowSurface(instance, window, nullptr, &surface));
            return surface;
            }, m_buffer);
        m_buffer[std::pair{ 0,0 }] = 'T';
        m_render.notify_update();
    }
    void run() {
        boost::asio::io_context io{};
        boost::asio::steady_timer timer{ io, 1ms };
        std::function<void(const boost::system::error_code)> timer_fun{
            [&timer, &timer_fun, &window_manager=m_window_manager, &io](const boost::system::error_code& err_code) {
                if (window_manager.run() == run_result::eBreak) {
                    io.stop();
                }
                else {
                    timer.expires_after(1ms);
                    timer.async_wait(timer_fun);
                }
            }
        };
        timer.async_wait(
            timer_fun
        );
        boost::asio::steady_timer render_timer{ io, 1000ms / 60 };
        std::function<void(const boost::system::error_code)> render_timer_complete{
            [&timer = render_timer, &complete = render_timer_complete, &render = m_render](const boost::system::error_code& err_code) {
                render.run();
                timer.expires_after(1000ms / 60);
                timer.async_wait(complete);
            }
        };
        render_timer.async_wait(render_timer_complete);
        m_window_manager.set_process_character_fun([this](uint32_t code) {
            m_buffer[{0, 1}] = code;
            m_render.notify_update();
            });

        SECURITY_ATTRIBUTES secu_attr{};
        secu_attr.bInheritHandle = TRUE;
        const char pipe_name[] = "\\\\.\\pipe\\terminal_emulator";
        HANDLE named_pipe_handle = CreateNamedPipe(pipe_name,
            PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE,
            1,
            256,
            256,
            0,
            &secu_attr
        );
        assert(named_pipe_handle != INVALID_HANDLE_VALUE);
        boost::asio::readable_pipe read_pipe{ io};
        read_pipe.assign(named_pipe_handle);
        std::array<char, 128> read_buf{};
        std::function<void(const boost::system::error_code&, std::size_t)> read_complete{
            [this, &read_buf, &read_pipe, &read_complete](const auto& error, auto bytes_transferred) {
                std::copy(read_buf.begin(),
                    read_buf.begin() + bytes_transferred,
                    m_buffer.begin());
                m_render.notify_update();
                read_pipe.async_read_some(boost::asio::mutable_buffer{ read_buf.data(), read_buf.size() }, read_complete);
            }
        };
        read_pipe.async_read_some(
            boost::asio::mutable_buffer{read_buf.data(), read_buf.size()},
            read_complete
            );
        HANDLE connect_event = CreateEvent(&secu_attr, TRUE, TRUE, NULL);
        assert(connect_event != NULL);
        OVERLAPPED overlapped{};
        overlapped.hEvent = connect_event;
        if (!ConnectNamedPipe(named_pipe_handle, &overlapped)) {
            assert(
                contains(
                    std::set{
                        ERROR_IO_PENDING,
                        ERROR_PIPE_CONNECTED
                    },
                    GetLastError()));
        }
        HANDLE write_pipe_handle = CreateFile(
            pipe_name,
            GENERIC_WRITE,
            0,
            &secu_attr,
            OPEN_EXISTING,
            0,
            NULL
        );
        auto err = GetLastError();
        assert(write_pipe_handle != INVALID_HANDLE_VALUE);
        STARTUPINFO si;
        PROCESS_INFORMATION pi;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        ZeroMemory(&pi, sizeof(pi));
        si.dwFlags |= STARTF_USESTDHANDLES;
        si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        si.hStdOutput = write_pipe_handle;
        si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
        // Start the child process. 
        if (!CreateProcess("Debug\\sh.exe",   // No module name (use command line)
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
        io.run();
    }
private:
    window_manager m_window_manager;
    vulkan_render m_render;
    multidimention_array<char, 32, 32> m_buffer;
};

int main() {
    try {
        terminal_emulator emulator;
        emulator.run();
    }
    catch (vk::SystemError& err) {
        std::cout << "vk::SystemError: " << err.what() << std::endl;
        return -1;
    }
    catch (std::exception& err) {
        std::cout << "std::exception: " << err.what() << std::endl;
    }
    catch (...) {
        std::cout << "unknown error\n";
        return -1;
    }
    return 0;
}
