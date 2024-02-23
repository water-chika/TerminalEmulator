enum os{
    eWindows,
    eLinux
};

namespace build_info {
    constexpr os runtime_os =
#if WIN32
           os::eWindows
#elif __unix__
           os::eLinux
#endif
           ;
};