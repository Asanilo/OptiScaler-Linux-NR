// Same-recording stage capture. Copies preserve source states and use actual queue fences.
#pragma once
#include "NrGpuLifetime.h"
#include <windows.h>
#include <d3d12.h>
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <locale>
#include <sstream>
#include <string>
#include <vector>

namespace capture
{
constexpr unsigned int kMaxFrames = 8;
constexpr unsigned long long kByteBudget = 768ull * 1024 * 1024;
struct Shot
{
    DlssNr::GpuLifetime::Token completion;
    ID3D12Resource* readback = nullptr;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout = {};
    unsigned long long bytes = 0;
    unsigned int frame = 0;
    DXGI_FORMAT resourceFormat = DXGI_FORMAT_UNKNOWN;
    DXGI_FORMAT interpretation = DXGI_FORMAT_UNKNOWN;
    std::string stage, colour;
};
class FrameCapture
{
  public:
    explicit FrameCapture(unsigned long long budget = kByteBudget) : budget_(std::min(budget, kByteBudget)) {}
    void request(unsigned int frames)
    {
        if (active_)
            return;
        release();
        wanted_ = std::min(frames, kMaxFrames);
        active_ = wanted_ != 0;
        error_.clear();
        status_ = active_ ? "Waiting for NR frames" : "No frames requested";
    }
    bool isActive() const { return active_; }
    unsigned int progress() const { return static_cast<unsigned int>(frames_.size()); }
    const std::string& status() const { return status_; }
    void beginFrame(std::string metadata)
    {
        if (!active_ || sealed_)
            return;
        if (open_)
            fail("unfinished_frame");
        if (sealed_)
            return;
        frames_.push_back(std::move(metadata));
        open_ = true;
    }
    void annotate(const std::string& fields)
    {
        if (!open_ || sealed_ || frames_.empty())
            return;
        auto& json = frames_.back();
        json.pop_back();
        json += "," + fields + "}";
    }
    void endFrame()
    {
        if (!open_)
            return;
        bool original = false, output = false;
        for (const auto& shot : shots_)
            if (shot.frame + 1 == frames_.size())
            {
                original |= shot.stage == "original";
                output |= shot.stage == "resolve" || shot.stage == "final_cached";
            }
        if (!original || !output)
        {
            fail("incomplete_frame_stages");
            return;
        }
        open_ = false;
        if (frames_.size() >= wanted_)
            sealed_ = true;
    }
    void fail(const char* reason)
    {
        if (!active_)
            return;
        error_ = reason;
        sealed_ = true;
        open_ = false;
        status_ = reason;
    }
    void record(ID3D12GraphicsCommandList* cmd, ID3D12Device* device, const char* stage, ID3D12Resource* source,
                D3D12_RESOURCE_STATES state, const char* colour)
    {
        if (!active_ || sealed_ || !open_)
            return;
        if (!source)
        {
            fail("missing_stage_resource");
            return;
        }
        auto desc = source->GetDesc();
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D || desc.SampleDesc.Count != 1 ||
            desc.DepthOrArraySize != 1 || desc.MipLevels != 1)
        {
            fail("unsupported_resource_layout");
            return;
        }
        // Resource format, copy-plane format and SRV interpretation may differ.
        // Preserve the original descriptor for GetCopyableFootprints. In particular,
        // a depth plane can be returned as R32_TYPELESS for a D32_FLOAT resource.
        auto interpretation = desc.Format;
        switch (desc.Format)
        {
        case DXGI_FORMAT_R16G16B16A16_TYPELESS:
            interpretation = DXGI_FORMAT_R16G16B16A16_FLOAT;
            break;
        case DXGI_FORMAT_R32G32B32A32_TYPELESS:
            interpretation = DXGI_FORMAT_R32G32B32A32_FLOAT;
            break;
        case DXGI_FORMAT_R10G10B10A2_TYPELESS:
            interpretation = DXGI_FORMAT_R10G10B10A2_UNORM;
            break;
        case DXGI_FORMAT_R8G8B8A8_TYPELESS:
            interpretation = DXGI_FORMAT_R8G8B8A8_UNORM;
            break;
        case DXGI_FORMAT_B8G8R8A8_TYPELESS:
            interpretation = DXGI_FORMAT_B8G8R8A8_UNORM;
            break;
        case DXGI_FORMAT_D32_FLOAT:
        case DXGI_FORMAT_R32_TYPELESS:
            interpretation = DXGI_FORMAT_R32_FLOAT;
            break;
        case DXGI_FORMAT_R32G32_TYPELESS:
            interpretation = DXGI_FORMAT_R32G32_FLOAT;
            break;
        case DXGI_FORMAT_R16G16_TYPELESS:
            interpretation = DXGI_FORMAT_R16G16_FLOAT;
            break;
        default:
            break;
        }
        for (const auto& previous : shots_)
            if (previous.stage == stage &&
                (previous.layout.Footprint.Width != desc.Width || previous.layout.Footprint.Height != desc.Height ||
                 previous.resourceFormat != desc.Format))
            {
                fail("resource_layout_changed");
                return;
            }
        Shot shot;
        shot.frame = static_cast<unsigned int>(frames_.size() - 1);
        shot.stage = stage;
        shot.colour = colour;
        shot.resourceFormat = desc.Format;
        shot.interpretation = interpretation;
        device->GetCopyableFootprints(&desc, 0, 1, 0, &shot.layout, nullptr, nullptr, &shot.bytes);
        if (!shot.bytes || shot.bytes > budget_ - bytes_)
        {
            fail("readback_budget_exceeded");
            return;
        }
        D3D12_HEAP_PROPERTIES heap = {};
        heap.Type = D3D12_HEAP_TYPE_READBACK;
        D3D12_RESOURCE_DESC buffer = {};
        buffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        buffer.Width = shot.bytes;
        buffer.Height = buffer.DepthOrArraySize = buffer.MipLevels = buffer.SampleDesc.Count = 1;
        buffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &buffer, D3D12_RESOURCE_STATE_COPY_DEST,
                                                   nullptr, IID_PPV_ARGS(&shot.readback))))
        {
            fail("readback_allocation_failed");
            return;
        }
        shot.completion = DlssNr::GpuLifetime::Begin(cmd);
        if (!shot.completion)
        {
            shot.readback->Release();
            fail("recording_not_tracked");
            return;
        }
        copy(cmd, source, state, shot);
        bytes_ += shot.bytes;
        shots_.push_back(std::move(shot));
    }
    // Called at the seam even when NR is disabled/skipped. Never wait on the render thread.
    std::string poll(const std::filesystem::path& root)
    {
        if (!active_)
            return {};
        for (const auto& shot : shots_)
            if (DlssNr::GpuLifetime::Discarded(shot.completion))
                fail("recording_discarded");
        if (!sealed_)
            return {};
        for (const auto& shot : shots_)
            if (!DlssNr::GpuLifetime::Discarded(shot.completion) &&
                !DlssNr::GpuLifetime::ReadbackReady(shot.completion))
                return {};
        std::error_code ec;
        std::filesystem::create_directories(root, ec);
        if (ec)
        {
            status_ = "capture_directory_failed: " + ec.message();
            release();
            return {};
        }
        std::filesystem::path directory;
        for (unsigned int attempt = 0; attempt < 10000; ++attempt)
        {
            directory = root / ("batch-" + std::to_string(GetCurrentProcessId()) + "-" +
                                std::to_string(GetTickCount64()) + "-" + std::to_string(attempt));
            if (std::filesystem::create_directory(directory, ec))
                break;
            if (ec || attempt == 9999)
            {
                status_ = "batch_directory_failed";
                release();
                return {};
            }
        }
        std::ostringstream manifest;
        manifest.imbue(std::locale::classic());
        manifest << std::setprecision(9) << "{\"schema_version\":2,\"requested_frames\":" << wanted_
                 << ",\"readback_bytes\":" << bytes_ << ",\"frames\":[";
        for (size_t i = 0; i < frames_.size(); ++i)
            manifest << (i ? "," : "") << frames_[i];
        manifest << "],\"images\":[";
        bool first = true;
        for (auto& shot : shots_)
        {
            const std::string name = std::to_string(shot.frame) + "-" + shot.stage + ".raw";
            const bool discarded = DlssNr::GpuLifetime::Discarded(shot.completion);
            bool written = false;
            if (!discarded)
            {
                void* mapped = nullptr;
                D3D12_RANGE range = { 0, static_cast<SIZE_T>(shot.bytes) };
                if (SUCCEEDED(shot.readback->Map(0, &range, &mapped)) && mapped)
                {
                    std::ofstream file(directory / name, std::ios::binary);
                    file.write(static_cast<const char*>(mapped), static_cast<std::streamsize>(shot.bytes));
                    file.close();
                    written = !file.fail();
                    D3D12_RANGE nothing = { 0, 0 };
                    shot.readback->Unmap(0, &nothing);
                    if (!written)
                        error_ = "raw_write_failed";
                }
                else
                    error_ = "readback_map_failed";
            }
            auto& fp = shot.layout.Footprint;
            manifest << (first ? "" : ",") << "{\"frame\":" << shot.frame << ",\"stage\":" << std::quoted(shot.stage)
                     << ",\"file\":" << std::quoted(name) << ",\"status\":"
                     << std::quoted(written     ? "ok"
                                    : discarded ? "discarded"
                                                : "failed")
                     << ",\"colour_space\":" << std::quoted(shot.colour) << ",\"width\":" << fp.Width
                     << ",\"height\":" << fp.Height << ",\"format\":" << static_cast<int>(shot.interpretation)
                     << ",\"resource_format\":" << static_cast<int>(shot.resourceFormat)
                     << ",\"copy_format\":" << static_cast<int>(fp.Format) << ",\"row_pitch\":" << fp.RowPitch
                     << ",\"offset\":" << shot.layout.Offset << ",\"bytes\":" << shot.bytes << "}";
            first = false;
        }
        manifest << "],\"status\":" << std::quoted(error_.empty() ? "complete" : error_) << "}\n";
        std::ofstream file(directory / "manifest.json", std::ios::binary);
        file << manifest.str();
        file.close();
        status_ = file.fail() ? "manifest_write_failed: " + directory.string()
                              : (error_.empty() ? "Saved: " : "Incomplete: ") + directory.string();
        release();
        return status_;
    }
    void release()
    {
        for (auto& shot : shots_)
            if (shot.readback)
                shot.readback->Release();
        shots_.clear();
        frames_.clear();
        bytes_ = 0;
        active_ = sealed_ = open_ = false;
    }

  private:
    static void copy(ID3D12GraphicsCommandList* cmd, ID3D12Resource* src, D3D12_RESOURCE_STATES state, Shot& shot)
    {
        if (shot.readback == nullptr)
            return;

        DlssNr::GpuLifetime::Hold(shot.completion, src);
        DlssNr::GpuLifetime::Hold(shot.completion, shot.readback);

        D3D12_RESOURCE_BARRIER b = {};
        b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        b.Transition.pResource = src;
        b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        b.Transition.StateBefore = state;
        b.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;

        const bool needsTransition = state != D3D12_RESOURCE_STATE_COPY_SOURCE;

        if (needsTransition)
            cmd->ResourceBarrier(1, &b);

        D3D12_TEXTURE_COPY_LOCATION dst = {};
        dst.pResource = shot.readback;
        dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        dst.PlacedFootprint = shot.layout;

        D3D12_TEXTURE_COPY_LOCATION source = {};
        source.pResource = src;
        source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        source.SubresourceIndex = 0;

        cmd->CopyTextureRegion(&dst, 0, 0, 0, &source, nullptr);

        if (needsTransition)
        {
            b.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
            b.Transition.StateAfter = state;
            cmd->ResourceBarrier(1, &b);
        }
    }

    std::vector<Shot> shots_;
    std::vector<std::string> frames_;
    const unsigned long long budget_;
    unsigned long long bytes_ = 0;
    unsigned int wanted_ = 0;
    bool active_ = false, sealed_ = false, open_ = false;
    std::string error_, status_;
};
} // namespace capture
