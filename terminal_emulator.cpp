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
        io.run();
        m_window_manager.set_process_character_fun([this](uint32_t code) {
            m_buffer[{0, 1}] = code;
            m_render.notify_update();
            });

        SECURITY_ATTRIBUTES secu_attr{};
        secu_attr.bInheritHandle = TRUE;
        HANDLE read_pipe, write_pipe;
        if (!CreatePipe(&read_pipe, &write_pipe, &secu_attr, 1024)) {
            throw std::runtime_error{ "can't create pipe" };
        }
        auto read_process_output{
            [read_pipe]() {
                char buf[8];
                DWORD read_count{ 0 };
                if (ReadFile(read_pipe, buf, sizeof(buf), &read_count, NULL)) {
                    std::cout << read_count << std::endl;
                }
                return run_result::eContinue;
            }
        };

        STARTUPINFO si;
        PROCESS_INFORMATION pi;

        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        ZeroMemory(&pi, sizeof(pi));
        si.dwFlags |= STARTF_USESTDHANDLES;
        si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        si.hStdOutput = write_pipe;
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
