#pragma once

#include <filesystem>

namespace AppPaths {
std::filesystem::path ExecutableDirectory();
std::filesystem::path DataDirectory();
} // namespace AppPaths
