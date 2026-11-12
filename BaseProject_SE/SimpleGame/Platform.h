#pragma once

#include <cstdint>
#include <filesystem>

namespace Platform
{
std::filesystem::path ExecutableDirectory();
std::uint64_t ProcessId();
bool ReplaceFileAtomically(const std::filesystem::path &source,
                           const std::filesystem::path &destination);
}
