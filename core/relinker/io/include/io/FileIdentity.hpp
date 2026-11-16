#ifndef IO_FILEIDENTITY_HPP
#define IO_FILEIDENTITY_HPP

#include <filesystem>

namespace Io {

bool SameFile(const std::filesystem::path& first, const std::filesystem::path& second);
std::filesystem::path ResolvedPath(const std::filesystem::path& path);
bool ResolvedPathIsWithin(const std::filesystem::path& path, const std::filesystem::path& root);

}

#endif
