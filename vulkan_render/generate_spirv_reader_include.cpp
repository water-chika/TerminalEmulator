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
}
void print_os_relate_member(std::ostream& out, auto& members, auto& valid_uses) {
    out << "class spirv_reader_os_member{" << std::endl;
    out << "public:" << std::endl;
    out << "spirv_reader_os_member(std::filesystem::path path){" << std::endl;
    std::ranges::for_each(valid_uses,
        [&out](auto& valid_use) {
        out << valid_use.name << "=" << valid_use.init << ";" << std::endl;
    });
    out << "}" << std::endl;
    out << "~spirv_reader_os_member(){" << std::endl;
    std::ranges::for_each(valid_uses,
        [&out](auto& valid_use) {
        out << valid_use.deinit << ";" << std::endl;
    });
    out << "}" << std::endl;
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
    struct func_call {
        std::string func;
        std::vector<std::string> args;
    };
    struct variable_valid_use {
        std::string name;
        std::string init;
        std::string deinit;
    };
    std::map<os, std::vector<variable_valid_use>> valid_use_map{
        {os::eWindows,
            {
                {
                    "hFile",
                    "CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)",
                    "CloseHandle(hFile)"
                },
                {
                    "hMapping",
                    "CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL)",
                    "CloseHandle(hMapping)"
                },
                {
                    "mmaped_ptr",
                    "MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0)",
                    "GetFileSize(hFile, NULL)",
                },
                {
                    "m_size",
                    "GetFileSize(hFile, NULL)",
                    "",
                }
            }
        },
        {os::eLinux, {}},
    };
    std::string out_path = argv[1];
    std::ofstream out{ out_path };
    print_include(out, include_map[build_info::runtime_os]);
    print_os_relate_member(out, member_map[build_info::runtime_os], valid_use_map[build_info::runtime_os]);
}