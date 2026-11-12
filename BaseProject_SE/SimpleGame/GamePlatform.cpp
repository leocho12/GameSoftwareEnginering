#include "stdafx.h"
#include "Platform.h"

#include <array>
#include <string>
#include <system_error>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <unistd.h>
#else
#include <unistd.h>
#endif

namespace Platform
{
std::filesystem::path ExecutableDirectory()
{
#ifdef _WIN32
    std::vector<wchar_t> buffer(32768);
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), DWORD(buffer.size()));
    if (length > 0 && length < buffer.size())
        return std::filesystem::path(buffer.data()).parent_path();
#elif defined(__APPLE__)
    std::uint32_t length = 0;
    _NSGetExecutablePath(nullptr, &length);
    if (length > 0)
    {
        std::vector<char> buffer(length + 1, '\0');
        if (_NSGetExecutablePath(buffer.data(), &length) == 0)
        {
            std::error_code error;
            auto path = std::filesystem::weakly_canonical(buffer.data(), error);
            if (!error)
                return path.parent_path();
            return std::filesystem::path(buffer.data()).parent_path();
        }
    }
#else
    std::array<char, 4096> buffer{};
    const auto length = readlink("/proc/self/exe", buffer.data(), buffer.size() - 1);
    if (length > 0)
        return std::filesystem::path(std::string(buffer.data(), std::size_t(length))).parent_path();
#endif

    std::error_code error;
    auto path = std::filesystem::current_path(error);
    return error ? std::filesystem::path(".") : path;
}

std::uint64_t ProcessId()
{
#ifdef _WIN32
    return static_cast<std::uint64_t>(GetCurrentProcessId());
#else
    return static_cast<std::uint64_t>(getpid());
#endif
}

bool ReplaceFileAtomically(const std::filesystem::path &source,
                           const std::filesystem::path &destination)
{
#ifdef _WIN32
    return MoveFileExW(source.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
#else
    std::error_code error;
    std::filesystem::rename(source, destination, error);
    return !error;
#endif
}
}
