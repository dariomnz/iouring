#pragma once

#include <cstring>
#include <iostream>

constexpr const char *file_name(const char *path) {
    const char *file = path;
    while (*path) {
        if (*path++ == '/') {
            file = path;
        }
    }
    return file;
}

#ifdef DEBUG
#define debug_error(out_format)                                                                                     \
    std::cerr << "[ERROR] [" << __func__ << "] [" << ::file_name(__FILE__) << ":" << __LINE__ << "] " << out_format \
              << std::endl;
#define debug_warning(out_format)                                                                                     \
    std::cerr << "[WARNING] [" << __func__ << "] [" << ::file_name(__FILE__) << ":" << __LINE__ << "] " << out_format \
              << std::endl;
#define debug_info(out_format)                                                                                     \
    std::cerr << "[INFO] [" << __func__ << "] [" << ::file_name(__FILE__) << ":" << __LINE__ << "] " << out_format \
              << std::endl;
#else
#define debug_error(out_format)
#define debug_warning(out_format)
#define debug_info(out_format)
#endif

#define print_error(out_format)                                                                                   \
    std::cerr << std::dec << "[ERROR] [" << ::file_name(__FILE__) << ":" << __LINE__ << "] [" << __func__ << "] " \
              << out_format << " : " << std::strerror(errno) << std::endl;
#define println(out_format) std::cout << out_format << std::endl;