#pragma once

#include <cstdint>
#include <filesystem>

namespace Platform
{
std::filesystem::path ExecutableDirectory();
std::uint64_t ProcessId();
}
