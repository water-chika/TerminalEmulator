#pragma once

#include <vector>
#include <filesystem>
#include <cassert>

#include "spirv_reader_os.hpp"

class spirv_file : protected spirv_reader_os_member{
public:
    spirv_file(std::filesystem::path path) {
#ifdef WIN32
        hFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        assert(hFile != INVALID_HANDLE_VALUE);
        hMapping = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
        assert(hMapping != INVALID_HANDLE_VALUE);
        mmaped_ptr = MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);
        assert(mmaped_ptr != nullptr);
        m_size = GetFileSize(hFile, NULL);
#endif
#ifdef __unix__
        m_file_descriptor = open(path.c_str(), O_RDONLY);
        m_size = lseek(m_file_descriptor, 0, SEEK_END);
        mmaped_ptr = mmap(NULL, m_size, PROT_READ, MAP_PRIVATE, m_file_descriptor, 0);
#endif
    }
    spirv_file(const spirv_file& file) = delete;
    spirv_file(spirv_file&& file) = delete;
    ~spirv_file() {
#ifdef WIN32
        UnmapViewOfFile(mmaped_ptr);
        CloseHandle(hMapping);
        CloseHandle(hFile);
#endif
#ifdef __unix__
        munmap(mmaped_ptr, m_size);
        close(m_file_descriptor);
#endif
    }
    spirv_file& operator=(const spirv_file& file) = delete;
    spirv_file& operator=(spirv_file&& file) = delete;

    const uint32_t* data() const {
        return static_cast<const uint32_t*>(mmaped_ptr);
    }
    // count of spirv code, that is uint32_t.
    size_t size() const {
        return m_size / 4;
    }
};
