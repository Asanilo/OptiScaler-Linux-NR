#pragma once
// Fixture environment only. Production shader, cache, timing and ownership code
// remain unchanged; application logger/config/state are supplied by this harness.
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <filesystem>

// Keep all log arguments type-checked; this fixture does not assert their text.
template <typename... Args> void NrCacheTestLog(const char*, const Args&...) {}
#define LOG_ERROR(...) NrCacheTestLog(__VA_ARGS__)
#define LOG_WARN(...) NrCacheTestLog(__VA_ARGS__)
#define LOG_INFO(...) NrCacheTestLog(__VA_ARGS__)
#define LOG_DEBUG(...) NrCacheTestLog(__VA_ARGS__)
#define SAFE_RELEASE(p)                                                                                                \
    do                                                                                                                 \
    {                                                                                                                  \
        if (p)                                                                                                         \
        {                                                                                                              \
            (p)->Release();                                                                                            \
            (p) = nullptr;                                                                                             \
        }                                                                                                              \
    } while (false)
namespace Util
{
inline void GetDeviceRemovedReason(ID3D12Device* device)
{
    std::printf("DeviceRemovedReason: %08lx\n", static_cast<unsigned long>(device->GetDeviceRemovedReason()));
}
inline std::filesystem::path DllPath()
{
    wchar_t path[32768] {};
    GetModuleFileNameW(nullptr, path, 32768);
    return path;
}
} // namespace Util
