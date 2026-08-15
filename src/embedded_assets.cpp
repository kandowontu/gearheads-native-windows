#include "embedded_assets.hpp"

#include "resource.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <climits>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>

namespace gh {
namespace {

constexpr std::array<std::uint8_t, 8> kPackMagic{
    'G', 'H', 'P', 'A', 'C', 'K', '1', '\0'
};

class Reader {
public:
    Reader(const void* data, std::size_t size)
        : current_(static_cast<const std::uint8_t*>(data)), end_(current_ + size) {}

    const std::uint8_t* take(std::size_t size) {
        if (size > remaining()) throw std::runtime_error("The embedded asset pack is truncated");
        const std::uint8_t* result = current_;
        current_ += size;
        return result;
    }

    std::uint16_t u16() {
        const auto* value = take(2);
        return static_cast<std::uint16_t>(value[0]) |
               static_cast<std::uint16_t>(value[1]) << 8U;
    }

    std::uint32_t u32() {
        const auto* value = take(4);
        return static_cast<std::uint32_t>(value[0]) |
               static_cast<std::uint32_t>(value[1]) << 8U |
               static_cast<std::uint32_t>(value[2]) << 16U |
               static_cast<std::uint32_t>(value[3]) << 24U;
    }

    std::uint64_t u64() {
        const auto* value = take(8);
        std::uint64_t result = 0;
        for (unsigned index = 0; index < 8; ++index) {
            result |= static_cast<std::uint64_t>(value[index]) << (index * 8U);
        }
        return result;
    }

    std::size_t remaining() const {
        return static_cast<std::size_t>(end_ - current_);
    }

private:
    const std::uint8_t* current_;
    const std::uint8_t* end_;
};

std::filesystem::path local_cache_base() {
    std::array<wchar_t, 32768> value{};
    const DWORD length = GetEnvironmentVariableW(
        L"LOCALAPPDATA", value.data(), static_cast<DWORD>(value.size())
    );
    if (length == 0 || length >= value.size()) {
        throw std::runtime_error("LOCALAPPDATA is unavailable for the runtime cache");
    }
    return std::filesystem::path(std::wstring(value.data(), length)) /
           "Gearheads Native" / "Cache";
}

std::wstring utf8_to_wide(const std::uint8_t* bytes, std::size_t size) {
    if (size == 0 || size > static_cast<std::size_t>(INT_MAX)) {
        throw std::runtime_error("The embedded asset pack contains an invalid path");
    }
    const int required = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        reinterpret_cast<const char*>(bytes),
        static_cast<int>(size),
        nullptr,
        0
    );
    if (required <= 0) throw std::runtime_error("An embedded asset path is not valid UTF-8");
    std::wstring result(static_cast<std::size_t>(required), L'\0');
    if (MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            reinterpret_cast<const char*>(bytes),
            static_cast<int>(size),
            result.data(),
            required
        ) != required) {
        throw std::runtime_error("Could not decode an embedded asset path");
    }
    return result;
}

std::filesystem::path safe_relative_path(const std::wstring& value) {
    std::filesystem::path result(value);
    if (result.empty() || result.is_absolute() || result.has_root_path()) {
        throw std::runtime_error("The embedded asset pack contains an absolute path");
    }
    for (const auto& component : result) {
        if (component == L"." || component == L".." || component.empty()) {
            throw std::runtime_error("The embedded asset pack contains an unsafe path");
        }
    }
    return result;
}

std::string digest_string(const std::uint8_t* digest) {
    std::ostringstream value;
    value << std::hex << std::setfill('0');
    for (std::size_t index = 0; index < 32; ++index) {
        value << std::setw(2) << static_cast<unsigned>(digest[index]);
    }
    return value.str();
}

void ensure_directory(const std::filesystem::path& path) {
    std::error_code error;
    std::filesystem::create_directories(path, error);
    if (error) throw std::runtime_error("Could not create the embedded asset cache");
}

void write_file(
    const std::filesystem::path& destination,
    const std::uint8_t* data,
    std::uint64_t size
) {
    ensure_directory(destination.parent_path());
    std::filesystem::path temporary = destination;
    temporary += L".tmp." + std::to_wstring(GetCurrentProcessId());
    HANDLE output = CreateFileW(
        temporary.c_str(),
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );
    if (output == INVALID_HANDLE_VALUE) {
        throw std::runtime_error("Could not create an extracted runtime file");
    }

    bool success = true;
    std::uint64_t written_total = 0;
    while (written_total < size) {
        const DWORD chunk = static_cast<DWORD>(
            std::min<std::uint64_t>(size - written_total, 1024U * 1024U)
        );
        DWORD written = 0;
        if (!WriteFile(output, data + written_total, chunk, &written, nullptr) || written != chunk) {
            success = false;
            break;
        }
        written_total += written;
    }
    if (!CloseHandle(output)) success = false;
    if (!success || !MoveFileExW(
            temporary.c_str(),
            destination.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH
        )) {
        DeleteFileW(temporary.c_str());
        throw std::runtime_error("Could not finalize an extracted runtime file");
    }
}

}  // namespace

std::filesystem::path materialize_embedded_assets(const std::filesystem::path& cache_base) {
    const HMODULE module = GetModuleHandleW(nullptr);
    const HRSRC resource = FindResourceW(
        module, MAKEINTRESOURCEW(IDR_EMBEDDED_ASSETS), RT_RCDATA
    );
    if (resource == nullptr) throw std::runtime_error("The embedded asset resource is missing");
    const HGLOBAL loaded = LoadResource(module, resource);
    const DWORD resource_size = SizeofResource(module, resource);
    const void* resource_data = loaded == nullptr ? nullptr : LockResource(loaded);
    if (resource_data == nullptr || resource_size < 48) {
        throw std::runtime_error("The embedded asset resource is invalid");
    }

    Reader reader(resource_data, resource_size);
    const auto* magic = reader.take(kPackMagic.size());
    if (std::memcmp(magic, kPackMagic.data(), kPackMagic.size()) != 0 || reader.u32() != 1) {
        throw std::runtime_error("The embedded asset pack has an unsupported format");
    }
    const std::uint32_t file_count = reader.u32();
    const auto* digest = reader.take(32);

    const std::filesystem::path base = cache_base.empty() ? local_cache_base() : cache_base;
    const std::filesystem::path cache = base / digest_string(digest);
    const std::filesystem::path assets = cache / "assets";
    const std::filesystem::path marker = cache / "complete.v1";
    if (std::filesystem::is_regular_file(marker) &&
        std::filesystem::is_regular_file(assets / "manifest.json")) {
        return assets;
    }

    ensure_directory(assets);
    for (std::uint32_t index = 0; index < file_count; ++index) {
        const std::uint16_t path_size = reader.u16();
        const std::uint64_t file_size = reader.u64();
        const auto* path_bytes = reader.take(path_size);
        const auto relative = safe_relative_path(utf8_to_wide(path_bytes, path_size));
        if (file_size > reader.remaining()) {
            throw std::runtime_error("The embedded asset pack contains an invalid file size");
        }
        const auto* contents = reader.take(static_cast<std::size_t>(file_size));
        write_file(assets / relative, contents, file_size);
    }
    if (reader.remaining() != 0) {
        throw std::runtime_error("The embedded asset pack contains trailing data");
    }

    std::ofstream complete(marker, std::ios::binary | std::ios::trunc);
    complete << "Gearheads Native embedded assets\n" << file_count << '\n';
    if (!complete) throw std::runtime_error("Could not complete the embedded asset cache");
    return assets;
}

}  // namespace gh
