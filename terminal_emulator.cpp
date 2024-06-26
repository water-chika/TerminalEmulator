#if WIN32
#define _WIN32_WINNT 0x0A00
#define NOMINMAX
#endif

#include "boost/asio.hpp"
#include "font_loader.hpp"
#include "vulkan_renderer.hpp"


#include <algorithm>
#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <random>
#include <set>
#include <strstream>
#include <thread>
#include <utility>
#include <fstream>

#include <GLFW/glfw3.h>

#include "multidimention_array.hpp"
#include "run_result.hpp"
#include "terminal_sequence_lexer.hpp"

#if WIN32
#include "named_pipe.hpp"
#include <ConsoleApi.h>
#endif

template<class T>
class add_glfw_error_print : public T {
public:
    add_glfw_error_print() {
        glfwSetErrorCallback([](int error, const char* msg) {
            std::cerr << "glfw: "
                << "(" << error << ") " << msg << std::endl;
            });
    }
};


template<class T>
class add_shared_vulkan_surface : public T {
public:
    using parent = T;
    add_shared_vulkan_surface() {
        m_surface = vk::SharedSurfaceKHR{ parent::get_vulkan_surface(), parent::get_vulkan_shared_instance() };
    }
    auto get_vulkan_shared_surface() {
        return m_surface;
    }
private:
    vk::SharedSurfaceKHR m_surface;
};

template<class T>
class add_glfw_vulkan_surface : public T {
public:
    using parent = T;
    add_glfw_vulkan_surface() {
        auto instance = parent::get_vulkan_instance();
        auto window = parent::get_glfw_window();
        VkSurfaceKHR surface{};
        auto res = glfwCreateWindowSurface(instance, window, NULL, &surface);
        if (res != VK_SUCCESS) {
            throw std::runtime_error{ "glfwCreateWindowSurface failed" };
        }
        m_surface = surface;
    }
    auto get_vulkan_surface() {
        return m_surface;
    }
private:
    vk::SurfaceKHR m_surface;
};

template<class T>
class add_glfw_library : public T {
public:
    using parent = T;
    add_glfw_library() {
        int ret = glfwInit();
        if (ret != GLFW_TRUE) {
            throw std::runtime_error{ "glfwInit failed" };
        }
    }
    ~add_glfw_library() {
        glfwTerminate();
    }
};

template<class T>
class window_manager : public T {
public:
  using parent = T;
  window_manager() : window{create_window()} {
    window_map.emplace(window, this);
    glfwSetCharCallback(window, character_callback);
    glfwSetKeyCallback(window, key_callback);
  }
  auto get_glfw_window() { return window; }
  void set_process_character_fun(auto&& fun) {
      process_character_fun = std::move(fun);
  }
  void process_character(uint32_t codepoint) {
    process_character_fun(codepoint);
  }
  static void character_callback(GLFWwindow *window, unsigned int codepoint) {
    auto manager = window_map[window];
    manager->process_character(codepoint);
  }
  static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
      auto manager = window_map[window];
      if (action == GLFW_PRESS) {
          if (key == GLFW_KEY_ENTER) {
              manager->process_character('\n');
          }
      }
  }
  run_result process_window_events() {
    glfwPollEvents();
    return glfwWindowShouldClose(window) ? run_result::eBreak
                                         : run_result::eContinue;
  }

private:
  static GLFWwindow *create_window() {
    uint32_t width = 1920, height = 1024;
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    return glfwCreateWindow(width, height, "Terminal Emulator", nullptr,
                            nullptr);
  }
  GLFWwindow *window;
  inline static std::map<GLFWwindow *, window_manager *> window_map; // C++17 inline static variable.
  std::function<void(uint32_t)> process_character_fun;
};

class terminal_buffer_manager {
public:
    terminal_buffer_manager() :
        m_buffer{ 82, 32 }
    {}
  auto &get_buffer() { return m_buffer; }
  void append_string(const std::string &str) {
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
    auto leave_size = m_buffer.get_dim0_size() - m_cursor_pos.first;
    auto current_pos = m_buffer.get_linear_index(m_cursor_pos);
    std::for_each(m_buffer.begin() + current_pos,
                  m_buffer.begin() + current_pos + leave_size,
                  [](auto &c) { c = ' '; });
    m_cursor_pos.second = (m_cursor_pos.second + 1) % m_buffer.get_dim1_size();
    m_cursor_pos.first = 0;
  }
  void append_str_data(const std::string_view str) {
    auto current_pos = m_buffer.get_linear_index(m_cursor_pos);
    assert(m_buffer.size() > current_pos);
    auto leave_size = m_buffer.size() - current_pos;
    auto count = std::min(str.size(), leave_size);
    std::copy(str.begin(), str.begin() + count, m_buffer.begin() + current_pos);
    if (str.size() > leave_size) {
      auto count = str.size() - leave_size;
      std::copy(str.begin() + leave_size, str.end(), m_buffer.begin());
    }

    auto& [x, y] = m_cursor_pos;
    x += str.size();
    y += x / m_buffer.get_dim0_size();
    x %= m_buffer.get_dim0_size();
    y %= m_buffer.get_dim1_size();
  }
#if WIN32
  COORD get_coord() {
      return COORD{ static_cast<int16_t>(m_buffer.get_width()), 
          static_cast<int16_t>(m_buffer.get_height()) };
  }
#endif

private:
  multidimention_vector<char> m_buffer;
  std::pair<int, int> m_cursor_pos;
};

template<class T>
class vulkan_instance : public T {
public:
  vulkan_instance() {
      auto application_info = vk::ApplicationInfo{}.setPApplicationName("Terminal Emulator").setApiVersion(vk::ApiVersion13);
      uint32_t count = 0; 
      auto extensions = glfwGetRequiredInstanceExtensions(&count);
      std::vector<const char*> enabled_extensions(extensions, extensions+count);
      auto required_extensions = std::array{
          vk::KHRGetPhysicalDeviceProperties2ExtensionName,
      };
      std::ranges::for_each(required_extensions,
          [&enabled_extensions](auto& ext){
              enabled_extensions.push_back(ext);
          }
      );
    auto create_info = vk::InstanceCreateInfo{}.setPEnabledExtensionNames(enabled_extensions).setPApplicationInfo(&application_info);
    m_instance = vk::SharedInstance{ vk::createInstance(create_info) };
  }
  auto get_vulkan_instance() {
    return *m_instance;
  }
  auto get_vulkan_shared_instance() {
      return m_instance;
  }
private:
  vk::SharedInstance m_instance;
};

class none_t {};

using namespace std::literals;
class terminal_emulator {
public:
  terminal_emulator(boost::asio::io_context& executor) : m_render{}, m_buffer_manager{} {
    m_render.init(
        m_buffer_manager.get_buffer());
    m_render.notify_update();

    //generated by attribute_dependence_parser from terminal_emulator_run.depend
#if WIN32
    using namespace windows;
    class pipe_async {
    public:
        pipe_async(terminal_emulator& emulator,
            boost::asio::io_context& executor,
            std::unique_ptr<boost::asio::readable_pipe>&& read_pipe) 
            : emulator{emulator},
            read_pipe{ std::move(read_pipe) }, lexer{},
            buf{std::make_unique<std::array<char, 128>>()}
        {
            async_read();
        }

        void operator()(const boost::system::error_code& err, std::size_t bytes_count) {
            process_text(bytes_count);
            async_read();
        }
        void process_text(std::size_t count) {
            std::vector<char> text = lexer.lex(std::string_view{ &(*buf)[0], count});
            emulator.m_buffer_manager.append_string(
                std::string{ text.data(), text.size()});
            emulator.m_render.notify_update();
            emulator.m_render.run();
        }
        void async_read() {
            auto mut_buf = boost::asio::mutable_buffer{ buf->data(), buf->size() };
            read_pipe->async_read_some(
                mut_buf,
                std::move(*this));
        }
    private:
        terminal_emulator& emulator;
        std::unique_ptr<boost::asio::readable_pipe> read_pipe;
        std::unique_ptr<std::array<char, 128>> buf;
        bool in_escape = false;
        terminal_sequence_lexer lexer;
    };
    auto input_pipe_name = generate_random_pipe_name();
    auto inputReadSide = create_named_pipe(input_pipe_name);
    auto inputWriteSide = std::make_shared<std::ofstream>(input_pipe_name);
    auto [outputReadSide, outputWriteSide] = create_pipe();
    HPCON hPC;
    HRESULT hr = CreatePseudoConsole(m_buffer_manager.get_coord(), inputReadSide, outputWriteSide, 0, &hPC);
    assert(SUCCEEDED(hr));
    STARTUPINFOEXW si;
    PrepareStartupInformation(hPC, &si);
    SetUpPseudoConsole(si, m_buffer_manager.get_coord());
    //auto shell = std::make_unique<process>("Debug/sh.exe", write_pipe_handle);
    auto read_pipe = std::make_unique<boost::asio::readable_pipe>(executor, outputReadSide);

    auto write_buf = std::make_shared<std::array<char, 10>>();

    m_render.set_process_character_fun(
        [inputWriteSide]
        (auto codepoint) mutable {
            auto& out = *inputWriteSide;
            *inputWriteSide << static_cast<char>(codepoint);
            out.flush();
        }
    );
    pipe_async pipe_async_v{ *this, executor, std::move(read_pipe) };

    class window_run {
    public:
        window_run(terminal_emulator& emulator, boost::asio::io_context& executor) :
            emulator{emulator},
            executor{executor},
            timer{ executor, 1ms }
        {
            async_run();
        }
        void operator()(const boost::system::error_code& err) {
            if (emulator.m_render.process_window_events() == run_result::eContinue) {
                async_run();
            }
            else {
                executor.stop();
            }
        }
        void async_run() {
            timer.expires_after(1ms);
            timer.async_wait(std::move(*this));
        }
    private:
        terminal_emulator& emulator;
        boost::asio::io_context& executor;
        boost::asio::steady_timer timer;
    };
    window_run window_run{ *this, executor };
#endif
  }
#if WIN32
  HRESULT PrepareStartupInformation(HPCON hpc, STARTUPINFOEXW* psi)
  {
      // Prepare Startup Information structure
      STARTUPINFOEXW si;
      ZeroMemory(&si, sizeof(si));
      si.StartupInfo.cb = sizeof(STARTUPINFOEXW);

      // Discover the size required for the list
      size_t bytesRequired;
      InitializeProcThreadAttributeList(NULL, 1, 0, &bytesRequired);

      // Allocate memory to represent the list
      si.lpAttributeList = (PPROC_THREAD_ATTRIBUTE_LIST)HeapAlloc(GetProcessHeap(), 0, bytesRequired);
      if (!si.lpAttributeList)
      {
          return E_OUTOFMEMORY;
      }

      // Initialize the list memory location
      if (!InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &bytesRequired))
      {
          HeapFree(GetProcessHeap(), 0, si.lpAttributeList);
          return HRESULT_FROM_WIN32(GetLastError());
      }

      // Set the pseudoconsole information into the list
      if (!UpdateProcThreadAttribute(si.lpAttributeList,
          0,
          PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
          hpc,
          sizeof(hpc),
          NULL,
          NULL))
      {
          HeapFree(GetProcessHeap(), 0, si.lpAttributeList);
          return HRESULT_FROM_WIN32(GetLastError());
      }

      *psi = si;

      return S_OK;
  }
  
  HRESULT SetUpPseudoConsole(STARTUPINFOEXW siEx, COORD size)
  {
      // ...

      PCWSTR childApplication = L"C:\\windows\\system32\\cmd.exe";

      // Create mutable text string for CreateProcessW command line string.
      const size_t charsRequired = wcslen(childApplication) + 1; // +1 null terminator
      PWSTR cmdLineMutable = (PWSTR)HeapAlloc(GetProcessHeap(), 0, sizeof(wchar_t) * charsRequired);

      if (!cmdLineMutable)
      {
          return E_OUTOFMEMORY;
      }

      wcscpy_s(cmdLineMutable, charsRequired, childApplication);

      PROCESS_INFORMATION pi;
      ZeroMemory(&pi, sizeof(pi));

      // Call CreateProcess
      if (!CreateProcessW(NULL,
          cmdLineMutable,
          NULL,
          NULL,
          FALSE,
          EXTENDED_STARTUPINFO_PRESENT,
          NULL,
          NULL,
          &siEx.StartupInfo,
          &pi))
      {
          HeapFree(GetProcessHeap(), 0, cmdLineMutable);
          return HRESULT_FROM_WIN32(GetLastError());
      }

      // ...
  }
#endif
private:
using mesh_pass = 
  renderer_presenter<
      mesh_renderer<
      add_shared_device<
      add_mesh_device_create_info_aggregate<
      set_queue_family_index<
      add_mesh_extension<
      vulkan_hpp_helper::add_swapchain_extension<
      vulkan_hpp_helper::add_empty_extensions<
      add_shared_physical_device<
      add_shared_vulkan_surface<
      add_glfw_vulkan_surface<
      window_manager<
      vulkan_instance<
      add_glfw_error_print<
      add_glfw_library<
      none_t
      >>>>>>>>>>>>>>>;
using vertex_pass =
renderer_presenter<
    vertex_renderer<
    add_shared_device<
    add_vertex_device_create_info_aggregate<
    set_queue_family_index<
    vulkan_hpp_helper::add_swapchain_extension<
    vulkan_hpp_helper::add_empty_extensions<
    add_shared_physical_device<
    add_shared_vulkan_surface<
    add_glfw_vulkan_surface<
    window_manager<
    vulkan_instance<
    add_glfw_error_print<
    add_glfw_library<
    none_t
    >>>>>>>>>>>>>>;
      vertex_pass m_render;
  terminal_buffer_manager m_buffer_manager;
};

int main() {
  try {
      boost::asio::io_context io{};
      terminal_emulator emulator{ io };
    io.run();
  } catch (vk::SystemError &err) {
    std::cout << "vk::SystemError: " << err.what() << std::endl;
    return -1;
  } catch (std::exception &err) {
    std::cout << "std::exception: " << err.what() << std::endl;
  } catch (...) {
    std::cout << "unknown error\n";
    return -1;
  }
  return 0;
}
