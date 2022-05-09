#ifndef FILE_UTILS_H
#define FILE_UTILS_H

#include <string>

inline bool exists(const std::string& name) {
    std::ifstream infile(name);
    return infile.good();
}

#endif
