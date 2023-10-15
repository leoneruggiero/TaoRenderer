#ifndef FILE_UTILS_H
#define FILE_UTILS_H

#include <string>
#include <filesystem>
namespace fs = std::filesystem;

inline bool exists(const std::string& name) {
    std::ifstream infile(name);
    return infile.good();
}

bool ContainsExtension(const std::string& dirName, const std::string& extName)
{
    std::string ext;
    for (const auto& entry : fs::directory_iterator(dirName))
    {
        if (entry.path().has_extension())
        {
            ext = entry.path().extension().string();
            break;
        }
    }
    return ext._Equal(extName);
}

#endif
