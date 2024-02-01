#pragma once

#include <vector>
#include <filesystem>
#include <cassert>

#include <Windows.h>

class spirv_file {
public:
    spirv_file(std::filesystem::path path) {
        hFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        assert(hFile != INVALID_HANDLE_VALUE);
        hMapping = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
        assert(hFile != INVALID_HANDLE_VALUE);
        mmaped_ptr = MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);
        assert(mmaped_ptr != nullptr);
    }
    spirv_file(const spirv_file& file) = delete;
    spirv_file(spirv_file&& file) = delete;
    ~spirv_file() {
        UnmapViewOfFile(mmaped_ptr);
        CloseHandle(hMapping);
        CloseHandle(hFile);
    }
    spirv_file& operator=(const spirv_file& file) = delete;
    spirv_file& operator=(spirv_file&& file) = delete;

    const uint32_t* data() const {
        return static_cast<const uint32_t*>(mmaped_ptr);
    }
    size_t size() const {
        return GetFileSize(hFile, NULL);
    }

private:
    HANDLE hFile;
    HANDLE hMapping;
    void* mmaped_ptr;
};