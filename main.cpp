#include <windows.h>
#include <filesystem>
#include <iostream>
#include <string>
#include <initializer_list>
#include <flat_set>
#include <thread>
#include <numeric>

#define MAX_THREADS 8ULL

namespace fs = std::filesystem;

std::size_t file_length(const char *const file_path) {
    HANDLE hFile = CreateFileA(file_path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return 0;

    LARGE_INTEGER file_size;
    if (!GetFileSizeEx(hFile, &file_size) || file_size.QuadPart == 0) {
        CloseHandle(hFile);
        return 0;
    }

    HANDLE hMap = CreateFileMappingA(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    if (!hMap) {
        CloseHandle(hFile);
        return 0;
    }

    const char *const pData = reinterpret_cast<const char *>(MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0));
    std::size_t count{1};
    if (pData) {
        const char *curr = pData, *const end = pData + file_size.QuadPart;
        while (curr < end && (curr = reinterpret_cast<const char *>(memchr(curr, '\n', end - curr)))) {
            ++curr;
            ++count;
        }

        UnmapViewOfFile(pData);
    }

    CloseHandle(hMap);
    CloseHandle(hFile);

    return count;
}

struct alignas(64) counter {
    std::size_t value;

    counter operator+(const counter& other) const {
        return {this->value + other.value};
    }
};

std::size_t calculate_length_of_files(const std::initializer_list<std::string_view>& path_names,
                                      const std::flat_set<std::string_view>& ignored_folders,
                                      const std::flat_set<std::string_view>& files_extensions) {
    const std::size_t paths_names_size = path_names.size(),
        threads_size = std::min(MAX_THREADS, paths_names_size), path_names_by_thread = paths_names_size / threads_size;
    counter counters[threads_size];

    {
        std::vector<std::jthread> threads{};
        threads.reserve(threads_size);

        std::initializer_list<std::string_view>::const_iterator it = path_names.begin();
        for (std::size_t i = 0; i < threads_size; ++i) {
            counters[i] = {0};

            threads.emplace_back([counter_ptr = &counters[i], &ignored_folders, &files_extensions, path_names = std::span<const std::string_view>{it, path_names_by_thread + ((i < threads_size - 1) ? 0 : paths_names_size % threads_size)}]() {
                for (const std::string_view& path_name : path_names) {
                    for (fs::recursive_directory_iterator it{path_name}; it != fs::recursive_directory_iterator{}; ++it) {
                        if (it->is_directory() && ignored_folders.contains(it->path().filename().string())) {
                            it.disable_recursion_pending();

                        } else if (it->is_regular_file()) {
                            if (files_extensions.contains(it->path().extension().string()))
                                counter_ptr->value += file_length(it->path().string().c_str());
                        }
                    }
                }
            });

            std::advance(it, path_names_by_thread);
        }
    }

    return std::accumulate(counters, counters + threads_size, counter{0ULL}).value;
}

int main() {
    std::cout << calculate_length_of_files(
        {"../../codewars_tasks", "../../leetcode_tasks", "../../collections_io_streams", "../../Codewars", "../../learn_cpp"},
        {"cmake-build-debug", ".idea", ".git"},
        {".h", ".c", ".hpp", ".cpp", ".tcc", ".ixx"}
     ) << "\n";

    return 0;
}
