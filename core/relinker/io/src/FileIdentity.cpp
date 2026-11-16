#include <io/FileIdentity.hpp>
#include <algorithm>
#include <system_error>

#ifdef _WIN32
#include <windows.h>
#include <array>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#endif

namespace Io {

#ifdef _WIN32
namespace {

using Identity = std::pair<std::uint64_t, std::array<std::uint8_t, 16>>;
using FileHandle = std::unique_ptr<void, decltype(&CloseHandle)>;

FileHandle OpenPath(const std::filesystem::path& path) {
    const auto handle = CreateFileW(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (handle == INVALID_HANDLE_VALUE)
        throw std::filesystem::filesystem_error("Cannot open file identity", path, std::error_code(static_cast<int>(GetLastError()), std::system_category()));
    return FileHandle(handle, &CloseHandle);
}

Identity FileIdentity(const std::filesystem::path& path) {
    const auto handle = OpenPath(path);
    Identity identity{};
    FILE_ID_INFO idInformation{};
    BY_HANDLE_FILE_INFORMATION information{};
    bool read = GetFileInformationByHandleEx(handle.get(), FileIdInfo, &idInformation, sizeof(idInformation)) != 0;
    if (read) {
        identity.first = idInformation.VolumeSerialNumber;
        std::copy(std::begin(idInformation.FileId.Identifier), std::end(idInformation.FileId.Identifier), identity.second.begin());
    } else {
        read = GetFileInformationByHandle(handle.get(), &information) != 0;
        identity.first = information.dwVolumeSerialNumber;
        const std::uint64_t index = (static_cast<std::uint64_t>(information.nFileIndexHigh) << 32) | information.nFileIndexLow;
        std::memcpy(identity.second.data(), &index, sizeof(index));
    }
    const auto error = GetLastError();
    if (!read) throw std::filesystem::filesystem_error("Cannot read file identity", path, std::error_code(static_cast<int>(error), std::system_category()));
    return identity;
}

}
#endif

bool SameFile(const std::filesystem::path& first, const std::filesystem::path& second) {
#ifdef _WIN32
    return FileIdentity(first) == FileIdentity(second);
#else
    for (const auto* path : {&first, &second}) {
        std::error_code error;
        const auto status = std::filesystem::status(*path, error);
        if (error) throw std::filesystem::filesystem_error("Cannot determine file identity", *path, error);
        if (!std::filesystem::exists(status)) {
            throw std::filesystem::filesystem_error(
                "Cannot determine file identity",
                *path,
                std::make_error_code(std::errc::no_such_file_or_directory)
            );
        }
    }
    return std::filesystem::equivalent(first, second);
#endif
}

std::filesystem::path ResolvedPath(const std::filesystem::path& path) {
#ifdef _WIN32
    const auto handle = OpenPath(path);
    std::wstring resolved(MAX_PATH, L'\0');
    DWORD volumeNaming = VOLUME_NAME_DOS;
    for (;;) {
        const auto length = GetFinalPathNameByHandleW(handle.get(), resolved.data(), static_cast<DWORD>(resolved.size()), FILE_NAME_NORMALIZED | volumeNaming);
        if (length == 0) {
            const auto error = GetLastError();
            if (volumeNaming == VOLUME_NAME_DOS && error == ERROR_PATH_NOT_FOUND) {
                volumeNaming = VOLUME_NAME_GUID;
                continue;
            }
            throw std::filesystem::filesystem_error("Cannot resolve path", path, std::error_code(static_cast<int>(error), std::system_category()));
        }
        if (length < resolved.size()) {
            resolved.resize(length);
            return resolved;
        }
        resolved.resize(length);
    }
#else
    return std::filesystem::canonical(path);
#endif
}

bool ResolvedPathIsWithin(const std::filesystem::path& path, const std::filesystem::path& root) {
    if (root.empty()) return false;
    auto end = root.end();
    if (!root.has_filename() && root != root.root_path()) --end;
    return std::mismatch(root.begin(), end, path.begin(), path.end()).first == end;
}

}
