#include <map>
#include <string>
#include <vector>
#include <iostream>
#include <ranges>
#include <algorithm>
#include <fstream>

#include "build_info.hpp"
void print_include(std::ostream& out, auto& include_paths) {
    std::ranges::for_each(include_paths,
    [&out](auto& path){
        out << "#include <" << path << ">" << std::endl;
    });

    if (build_info::runtime_os == os::eWindows) {
        out << "int open(const char*, ...){ return 0;}" << std::endl;
        out << "int lseek(int, ...){ return 0;}" << std::endl;
        out << "void* mmap(int, ...){ return NULL;}" << std::endl;
    }
}
void print_os_relate_member(std::ostream& out, auto& members) {
    out << "class spirv_reader_os_member{" << std::endl;
    out << "public:" << std::endl;
    std::ranges::for_each(members, 
        [&out](auto& member) {
            out << member.type << " " << member.name << ";" << std::endl;
        });
    out << "};" << std::endl;
}
int main(int argc, char** argv) {
    std::map<os, std::vector<std::string>> include_map{
        {os::eWindows, {"Windows.h"}},
        {os::eLinux, {"sys/mman.h", "fcntl.h", "unistd.h"}},
    };
    struct class_member {
        std::string type;
        std::string name;
    };
    std::map<os, std::vector<class_member>> member_map{
        {os::eWindows, {{"HANDLE", "hFile"},{"HANDLE", "hMapping"}, {"void*", "mmaped_ptr"}, {"size_t", "m_size"}}},
        {os::eLinux, {{"int", "m_file_descriptor"},{"void*", "mmaped_ptr"},{"size_t", "m_size"}}},
    };
    std::string out_path = argv[1];
    std::ofstream out{ out_path };
    print_include(out, include_map[build_info::runtime_os]);
    print_os_relate_member(out, member_map[build_info::runtime_os]);
}