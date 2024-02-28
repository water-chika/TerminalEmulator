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
#include <algorithm>
#include <random>

#include "multidimention_array.hpp"
#include "run_result.hpp"
#include "named_pipe.hpp"


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
    auto create_surface(vk::Instance instance) {
        VkSurfaceKHR surface{};
        auto res = glfwCreateWindowSurface(instance, window, NULL, &surface);
        return surface;
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

class terminal_buffer_manager {
public:
    auto& get_buffer() {
        return m_buffer;
    }
    void append_string(const std::string& str) {
        auto line_begin = str.begin();
        while (true) {
            auto line_end = std::find(line_begin, str.end(), '\n');
            std::string_view line{line_begin, line_end};
            append_str_data(line);
            if (line_end == str.end()) {
                break;
            }
            line_begin = line_end + 1;
            new_line();
        }
    }
    void append_line(const std::string_view str) {
        append_str_data(str);
        new_line();
    }
    void new_line() {
        auto& [x, y] = m_cursor_pos;
        auto leave_size = m_buffer.get_dim0_size() - m_cursor_pos.first;
        auto current_pos = y * 32 + x;
        std::for_each(m_buffer.begin() + current_pos, m_buffer.begin() + current_pos + leave_size, [](auto& c) {
            c = ' ';
            });
        m_cursor_pos.second = (m_cursor_pos.second + 1) % m_buffer.get_dim1_size();
        m_cursor_pos.first = 0;
    }
    void append_str_data(const std::string_view str) {
        auto& [x, y] = m_cursor_pos;
        auto current_pos = y * 32 + x;
        assert(m_buffer.size() > current_pos);
        auto leave_size = m_buffer.size() - current_pos;
        auto count = std::min(str.size(), leave_size);
        std::copy(str.begin(),
            str.begin() + count,
            m_buffer.begin() + current_pos);
        if (str.size() > leave_size) {
            auto count = str.size() - leave_size;
            std::copy(str.begin() + leave_size,
                str.end(),
                m_buffer.begin());
        }
        x += str.size();
        y += x / m_buffer.get_dim0_size();
        x %= m_buffer.get_dim0_size();
        y %= m_buffer.get_dim1_size();
    }
private:
    multidimention_array<char, 32, 16> m_buffer;
    std::pair<int, int> m_cursor_pos;
};

using namespace std::literals;
class terminal_emulator {
public:
    terminal_emulator() :
        m_window_manager{}, m_render{}, m_buffer_manager{} {
        std::string str = "hello world! Wow, do you think this is a good start? ...............abcdefghijklmnopqrstuvwxyz";
        m_buffer_manager.append_string(str);

        m_render.init([this](vk::Instance instance) {
            return m_window_manager.create_surface(instance);
            }, m_buffer_manager.get_buffer());
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
        m_window_manager.set_process_character_fun(
            [this](uint32_t code) {
            });

        auto [read_pipe_handle, write_pipe_handle] = windows::create_pipe();
        windows::process shell{ "Debug/sh.exe", write_pipe_handle };
        boost::asio::readable_pipe read_pipe{ io, read_pipe_handle };

        std::array<char, 128> read_buf{};
        std::function<void(const boost::system::error_code&, std::size_t)> read_complete{
            [this, &read_buf, &read_pipe, &read_complete](const auto& error, auto bytes_transferred) {
                m_buffer_manager.append_string(std::string{read_buf.data(), bytes_transferred});
                m_render.notify_update();
                read_pipe.async_read_some(boost::asio::mutable_buffer{ read_buf.data(), read_buf.size() }, read_complete);
            }
        };
        read_pipe.async_read_some(
            boost::asio::mutable_buffer{read_buf.data(), read_buf.size()},
            read_complete
            );
        io.run();
    }
private:
    window_manager m_window_manager;
    vulkan_render m_render;
    terminal_buffer_manager m_buffer_manager;
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
