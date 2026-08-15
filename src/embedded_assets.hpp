#pragma once

#include <filesystem>

namespace gh {

// Materializes the converted runtime files embedded in this executable.  The
// optional cache base exists for verification tools; the game uses LocalAppData.
std::filesystem::path materialize_embedded_assets(
    const std::filesystem::path& cache_base = {}
);

}  // namespace gh
