// Real RTX D3D12 execution of the production Sky cache wrapper, DXBC, base
// shader helpers and submission lifetime. Controlled NR input;
// this does not validate the vendor model or game appearance.
#include "standalone/cache/pch.h"
#include <dxgi1_4.h>
#include <stdexcept>
#include <functional>
#include "../OptiScaler/shaders/dlssnr/DlssNr_EditCache_Dx12.h"
#include "../OptiScaler/gpu_time/GpuTime_Dx12.h"

namespace
{
void Check(HRESULT hr, const char* label)
{
    if (FAILED(hr))
    {
        std::printf("%s: %08lx\n", label, static_cast<unsigned long>(hr));
        throw std::runtime_error(label);
    }
}
void Require(bool ok, const char* label)
{
    if (!ok)
        throw std::runtime_error(label);
}
constexpr auto kSrv = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
constexpr auto kUav = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
ID3D12Device* device = nullptr;
ID3D12CommandQueue* queue = nullptr;
ID3D12GraphicsCommandList* list = nullptr;
ID3D12CommandAllocator* allocator = nullptr;
ID3D12Fence* fence = nullptr;
uint64_t fenceValue = 0;
std::vector<ID3D12Resource*> owners;
unsigned int width = 17, height = 13;
ID3D12Resource* original = nullptr;
ID3D12Resource* target = nullptr;
ID3D12Resource* keep = nullptr;
ID3D12Resource* depth = nullptr;
ID3D12Resource* motion = nullptr;
bool breakFresh = false, breakRegional = false, breakJitter = false;

ID3D12Resource* Buffer(UINT64 bytes, D3D12_HEAP_TYPE type, D3D12_RESOURCE_STATES state)
{
    auto heap = CD3DX12_HEAP_PROPERTIES(type);
    auto desc = CD3DX12_RESOURCE_DESC::Buffer(bytes);
    ID3D12Resource* resource = nullptr;
    Check(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, state, nullptr, IID_PPV_ARGS(&resource)),
          "buffer");
    owners.push_back(resource);
    return resource;
}
ID3D12Resource* Texture(DXGI_FORMAT format, D3D12_RESOURCE_STATES state)
{
    auto heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    auto desc =
        CD3DX12_RESOURCE_DESC::Tex2D(format, width, height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    ID3D12Resource* resource = nullptr;
    Check(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, state, nullptr, IID_PPV_ARGS(&resource)),
          "texture");
    owners.push_back(resource);
    return resource;
}
void Barrier(ID3D12Resource* resource, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to)
{
    auto b = CD3DX12_RESOURCE_BARRIER::Transition(resource, from, to);
    list->ResourceBarrier(1, &b);
}
void Submit()
{
    Check(list->Close(), "close");
    ID3D12CommandList* commands[] { list };
    queue->ExecuteCommandLists(1, commands); // Actual production Execute/Signal adapter.
    Check(queue->Signal(fence, ++fenceValue), "signal");
    HANDLE event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    Require(event != nullptr, "event");
    Check(fence->SetEventOnCompletion(fenceValue, event), "fence event");
    const auto wait = WaitForSingleObject(event, 30000);
    CloseHandle(event);
    Require(wait == WAIT_OBJECT_0, "GPU timeout");
    Check(allocator->Reset(), "allocator reset");
    Check(list->Reset(allocator, nullptr), "list reset"); // Retires the actual previous recording.
    DlssNr::GpuLifetime::Poll();
}
void Fill(ID3D12Resource* resource, D3D12_RESOURCE_STATES idle, unsigned int channels,
          const std::function<float(unsigned int, unsigned int, unsigned int)>& value)
{
    const auto desc = resource->GetDesc();
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout {};
    UINT64 bytes = 0;
    device->GetCopyableFootprints(&desc, 0, 1, 0, &layout, nullptr, nullptr, &bytes);
    auto* upload = Buffer(bytes, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
    char* data = nullptr;
    D3D12_RANGE noRead { 0, 0 };
    Check(upload->Map(0, &noRead, reinterpret_cast<void**>(&data)), "upload map");
    std::memset(data, 0, static_cast<size_t>(bytes));
    for (unsigned int y = 0; y < desc.Height; ++y)
        for (unsigned int x = 0; x < desc.Width; ++x)
            for (unsigned int c = 0; c < channels; ++c)
                (reinterpret_cast<float*>(data + layout.Offset + y * layout.Footprint.RowPitch) + x * channels)[c] =
                    value(x, y, c);
    upload->Unmap(0, nullptr);
    Barrier(resource, idle, D3D12_RESOURCE_STATE_COPY_DEST);
    D3D12_TEXTURE_COPY_LOCATION dst {};
    dst.pResource = resource;
    dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    D3D12_TEXTURE_COPY_LOCATION src {};
    src.pResource = upload;
    src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    src.PlacedFootprint = layout;
    list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
    Barrier(resource, D3D12_RESOURCE_STATE_COPY_DEST, idle);
}
void Uniform(ID3D12Resource* resource, D3D12_RESOURCE_STATES idle, unsigned int channels, float value)
{
    Fill(resource, idle, channels, [=](unsigned int, unsigned int, unsigned int c) { return c == 3 ? 0.375f : value; });
}
std::vector<float> Read(ID3D12Resource* resource, D3D12_RESOURCE_STATES idle, unsigned int channels)
{
    const auto desc = resource->GetDesc();
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout {};
    UINT64 bytes = 0;
    device->GetCopyableFootprints(&desc, 0, 1, 0, &layout, nullptr, nullptr, &bytes);
    auto* readback = Buffer(bytes, D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COPY_DEST);
    Barrier(resource, idle, D3D12_RESOURCE_STATE_COPY_SOURCE);
    D3D12_TEXTURE_COPY_LOCATION src {};
    src.pResource = resource;
    src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    D3D12_TEXTURE_COPY_LOCATION dst {};
    dst.pResource = readback;
    dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    dst.PlacedFootprint = layout;
    list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
    Barrier(resource, D3D12_RESOURCE_STATE_COPY_SOURCE, idle);
    Submit();
    char* data = nullptr;
    D3D12_RANGE range { 0, static_cast<SIZE_T>(bytes) };
    Check(readback->Map(0, &range, reinterpret_cast<void**>(&data)), "readback map");
    std::vector<float> result(static_cast<size_t>(desc.Width) * desc.Height * channels);
    for (unsigned int y = 0; y < desc.Height; ++y)
        std::memcpy(result.data() + y * desc.Width * channels, data + layout.Offset + y * layout.Footprint.RowPitch,
                    static_cast<size_t>(desc.Width) * channels * sizeof(float));
    D3D12_RANGE noWrite { 0, 0 };
    readback->Unmap(0, &noWrite);
    return result;
}
DlssNrCacheInputs Inputs(float mv = 0.0f, float z = 0.5f)
{
    Uniform(depth, kSrv, 1, z);
    Fill(motion, kSrv, 2, [=](unsigned int, unsigned int, unsigned int c) { return c == 0 ? mv : 0.0f; });
    DlssNrCacheInputs in {};
    in.depth = depth;
    in.motion = motion;
    in.depthWidth = in.motionWidth = width;
    in.depthHeight = in.motionHeight = height;
    in.passthrough = true;
    return in;
}
void TimingCases()
{
    GpuTime_Dx12 timer(device, true);
    // More recordings than the old three-slot ring, without reading anything.
    // Every completed-but-unread query must remain available, independent of phase.
    for (uint64_t frame = 1; frame <= 8; ++frame)
    {
        timer.Start(list);
        timer.End(list, frame);
        Submit();
    }
    const auto complete = timer.ReadCompletedGpuTimes();
    Require(complete.size() == 8, "timer retains all completed unread refresh/cache phases");
    for (size_t i = 0; i < complete.size(); ++i)
        Require(complete[i].tag == i + 1 && std::isfinite(complete[i].ms) && complete[i].ms >= 0,
                "timer returns actual queue timestamps with exact frame identity");
    Require(timer.ReadCompletedGpuTimes().empty(), "timer query consumed exactly once");
    // One still-executable recording: no query may be read or overwritten early.
    for (uint64_t frame = 9; frame <= 40; ++frame)
    {
        timer.Start(list);
        timer.End(list, frame);
    }
    Require(timer.ReadCompletedGpuTimes().empty(), "unsubmitted query cannot be read");
    Submit();
    const auto bounded = timer.ReadCompletedGpuTimes();
    Require(bounded.size() == 16, "full query ring drops telemetry without overwriting GPU work");
    for (size_t i = 0; i < bounded.size(); ++i)
        Require(bounded[i].tag == i + 9, "pending query identities survive ring exhaustion");
    timer.Start(list);
    timer.End(list, 41);
    Submit();
    const auto recovered = timer.ReadCompletedGpuTimes();
    Require(recovered.size() == 1 && recovered[0].tag == 41, "timer recovers after real GPU completion");
    timer.Start(list);
    timer.End(list, 42);
    Check(list->Close(), "timing discard close");
    Check(allocator->Reset(), "timing discard allocator reset");
    Check(list->Reset(allocator, nullptr), "timing discard list reset");
    Require(timer.ReadCompletedGpuTimes().empty(), "discarded timestamps are never reported");
    timer.Start(list);
    timer.End(list, 43);
    Submit();
    Require(timer.ReadCompletedGpuTimes().size() == 1, "timer recovers after discard");
    GpuTime_Dx12 legacy(device);
    for (int frame = 0; frame < 4; ++frame)
    {
        legacy.Start(list);
        legacy.End(list);
        Submit();
    }
    Require(legacy.ReadGpuTime(queue).has_value(), "non-NR three-slot timer remains usable");
    std::puts(
        "PASS: production GPU timer drains both phases, exact identities, bounded slots, discard and legacy path");
}
void AllocateFrames()
{
    original = Texture(DXGI_FORMAT_R32G32B32A32_FLOAT, kSrv);
    target = Texture(DXGI_FORMAT_R32G32B32A32_FLOAT, kUav);
    keep = Texture(DXGI_FORMAT_R32G32B32A32_FLOAT, kUav);
    depth = Texture(DXGI_FORMAT_R32_FLOAT, kSrv);
    motion = Texture(DXGI_FORMAT_R32G32_FLOAT, kSrv);
}
void Near(float actual, float expected, float tolerance, const char* label)
{
    std::printf("%s: %.6f (expected %.6f +/- %.6f)\n", label, actual, expected, tolerance);
    Require(std::isfinite(actual) && std::abs(actual - expected) <= tolerance, label);
}
void Alpha(const std::vector<float>& frame)
{
    for (size_t i = 0; i < frame.size(); i += 4)
    {
        Require(std::isfinite(frame[i]) && frame[i] >= 0.0f, "finite RGB");
        Require(std::abs(frame[i + 3] - 0.375f) < 0.0001f, "fresh game alpha preserved");
    }
}
std::vector<float> Frame(DlssNrEditCache_Dx12& cache, Config& cfg, bool wantRefresh, float orig, float raw,
                         bool reset = false, bool preSr = false, float mv = 0.0f, float z = 0.5f, bool inverted = false)
{
    Uniform(original, kSrv, 4, orig);
    auto in = Inputs(mv, z);
    in.depthInverted = inverted;
    Require(cache.BeginFrame(list, cfg, device, width, height, DXGI_FORMAT_R32G32B32A32_FLOAT, reset, preSr) ==
                wantRefresh,
            "actual wrapper cadence");
    Require(cache.ActiveThisFrame(), "cache resources reserved");
    if (wantRefresh)
    {
        bool resetModel = false;
        cache.ModelMotion(list, device, in, resetModel);
        Uniform(target, kUav, 4, raw); // Controlled model answer; NGX deliberately not loaded.
        Require(cache.CaptureRefresh(list, device, target, original, in), "capture refresh");
    }
    else
    {
        if (!breakFresh)
            Require(cache.CopyIn(list, original, target), "fresh pre-SR colour copy");
        Require(cache.RunCached(list, device, target, keep, in), "cached output recorded");
    }
    cache.EndFrame(list);
    auto result = Read(target, kUav, 4);
    Alpha(result);
    return result;
}
} // namespace

int main(int argc, char** argv)
{
    if (argc < 3 || std::strcmp(argv[1], "--report") != 0 || !std::freopen(argv[2], "w", stdout))
        return 2;
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    try
    {
        if (argc == 4)
        {
            breakFresh = std::strcmp(argv[3], "--break-fresh") == 0;
            breakRegional = std::strcmp(argv[3], "--break-regional") == 0;
            breakJitter = std::strcmp(argv[3], "--break-jitter") == 0;
            Require(breakFresh || breakRegional || breakJitter, "unknown negative control");
        }
        IDXGIFactory1* factory = nullptr;
        Check(CreateDXGIFactory1(IID_PPV_ARGS(&factory)), "factory");
        IDXGIAdapter1* adapter = nullptr;
        for (UINT i = 0; SUCCEEDED(factory->EnumAdapters1(i, &adapter)); ++i)
        {
            DXGI_ADAPTER_DESC1 desc {};
            adapter->GetDesc1(&desc);
            if (desc.VendorId == 0x10de &&
                SUCCEEDED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device))))
            {
                std::printf("NVIDIA: device=%04x\n", desc.DeviceId);
                adapter->Release();
                break;
            }
            adapter->Release();
            adapter = nullptr;
        }
        factory->Release();
        Require(device != nullptr, "NVIDIA D3D12 device required");
        D3D12_COMMAND_QUEUE_DESC q {};
        q.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        Check(device->CreateCommandQueue(&q, IID_PPV_ARGS(&queue)), "queue");
        Check(device->CreateCommandAllocator(q.Type, IID_PPV_ARGS(&allocator)), "allocator");
        Check(device->CreateCommandList(0, q.Type, allocator, nullptr, IID_PPV_ARGS(&list)), "list");
        Check(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)), "fence");
        if (!breakFresh && !breakRegional && !breakJitter)
            TimingCases();
        AllocateFrames();
        Config cfg;
        cfg.DlssNrCacheInterval = 2;
        cfg.DlssNrCacheAdaptive = false;
        cfg.DlssNrCacheHighDecay = 1;
        cfg.DlssNrCacheStabilize = 0;
        cfg.DlssNrCacheTemporal = 0;
        cfg.DlssNrCacheLowTemporal = 0;
        cfg.DlssNrCacheDespeckle = false;
        cfg.DlssNrCacheCrossfade = false;
        {
            DlssNrEditCache_Dx12 cache(device);
            Require(cache.IsInit(), "production cache shader initializes");
            Frame(cache, cfg, true, 1, 2);
            const auto fresh = Frame(cache, cfg, false, 1.1f, 0);
            Near(fresh[0], (1.1f + 1.0f / 512) * ((2.0f + 1.0f / 512) / (1.0f + 1.0f / 512)) - 1.0f / 512, 0.015f,
                 "cached edit uses fresh game frame, not old pixels");
            for (int i = 2; i < 20; ++i)
                Frame(cache, cfg, (i % 2) == 0, 1, 1.2f);
            auto status = cache.GetStatus();
            Require(status.refreshes == 10 && status.cached == 10, "real dispatch refresh/cache counters");
            Frame(cache, cfg, true, 1, 1.2f, true);
            Near(Frame(cache, cfg, false, 1, 0, false, false, 0, 0.01f)[0], 1, 0.015f,
                 "depth discontinuity rejects cached edit");
            Frame(cache, cfg, true, 1, 1.2f, true);
            Near(Frame(cache, cfg, false, 1, 0, false, false, 64)[0], 1, 0.015f,
                 "out-of-frame motion rejects cached edit");
            Frame(cache, cfg, true, 1, 1.2f, true, false, 0, 0.75f, true);
            Near(Frame(cache, cfg, false, 1, 0, false, false, 0, 0.75f, true)[0], 1.2f, 0.015f,
                 "reversed-Z valid history reprojects");
            Frame(cache, cfg, true, 1, 1.2f, false, false, 0, 0.75f, true);
            Near(Frame(cache, cfg, false, 1, 0, false, false, 0, 0.99f, true)[0], 1, 0.015f,
                 "reversed-Z depth change rejects cached edit");
            cfg.DlssNrCacheDebugView = 1;
            Frame(cache, cfg, true, 1, 1.2f, true);
            const auto colour = Frame(cache, cfg, false, 10, 0);
            Near(colour[0], 0.2f, 0.005f, "colour change lowers detail confidence (red)");
            Near(colour[1], 0.3f, 0.005f, "colour change lowers detail confidence (green)");
            cfg.DlssNrCacheDebugView = 0;
            cfg.DlssNrCacheInterval = 1;
            cfg.DlssNrCacheLowTemporal = breakRegional ? 0.0f : 0.95f;
            Frame(cache, cfg, true, 1, 1.05f, true, true);
            const auto regional = Frame(cache, cfg, true, 1, 1.12f, false, true);
            std::printf("pre-SR regional-only: %.6f (fresh NR 1.120000)\n", regional[0]);
            Require(regional[0] > 1.04f && regional[0] < 1.10f,
                    "pre-SR low-only temporal path smooths regional breathing");
            Near(Frame(cache, cfg, true, 1, 1.2f, true, true)[0], 1.2f, 0.015f, "reset rejects temporal history");

            cfg.DlssNrCacheLowTemporal = 0;
            cfg.DlssNrCacheInterval = 2;
            Uniform(original, kSrv, 4, 1);
            auto in = Inputs();
            Require(cache.BeginFrame(list, cfg, device, width, height, DXGI_FORMAT_R32G32B32A32_FLOAT, true, true),
                    "jitter seed refresh");
            Fill(target, kUav, 4,
                 [](unsigned int x, unsigned int, unsigned int c) { return c == 3  ? 0.375f
                                                                           : x < 8 ? 1.05f
                                                                                   : 1.2f; });
            Require(cache.CaptureRefresh(list, device, target, original, in), "jitter seed capture");
            cache.EndFrame(list);
            Read(target, kUav, 4);
            in = Inputs();
            in.jitterDeltaX = breakJitter ? 0.0f : 1.0f / width;
            Require(!cache.BeginFrame(list, cfg, device, width, height, DXGI_FORMAT_R32G32B32A32_FLOAT, false, true),
                    "jitter cached frame");
            Require(cache.CopyIn(list, original, target), "jitter fresh copy");
            Require(cache.RunCached(list, device, target, keep, in), "jitter reproject");
            cache.EndFrame(list);
            const auto jitter = Read(target, kUav, 4);
            Near(jitter[(height / 2 * width + 7) * 4], 1.2f, 0.015f, "jitter shifts edit across boundary");
            Alpha(jitter);

            // Accumulate vectors in their original units over skipped frames.
            cache.Invalidate();
            in = Inputs(1);
            Require(cache.BeginFrame(list, cfg, device, width, height, DXGI_FORMAT_R32G32B32A32_FLOAT, true),
                    "motion seed");
            bool resetModel = false;
            auto* acc = cache.ModelMotion(list, device, in, resetModel);
            Require(acc && !resetModel, "motion accumulator available");
            Uniform(target, kUav, 4, 1.2f);
            Require(cache.CaptureRefresh(list, device, target, original, in), "motion seed capture");
            cache.EndFrame(list);
            Read(target, kUav, 4);
            in = Inputs(1);
            Require(!cache.BeginFrame(list, cfg, device, width, height, DXGI_FORMAT_R32G32B32A32_FLOAT, false),
                    "motion skipped frame");
            Require(cache.CopyIn(list, original, target), "motion fresh copy");
            Require(cache.RunCached(list, device, target, keep, in), "motion cached");
            cache.EndFrame(list);
            Read(target, kUav, 4);
            in = Inputs(1);
            Require(cache.BeginFrame(list, cfg, device, width, height, DXGI_FORMAT_R32G32B32A32_FLOAT, false),
                    "motion refresh");
            acc = cache.ModelMotion(list, device, in, resetModel);
            Require(acc && !resetModel, "accumulated model motion");
            // EndFrame returns the accumulated vectors from SRV to UAV before readback.
            cache.EndFrame(list);
            const auto accumulated = Read(acc, kUav, 2);
            Near(accumulated[(height / 2 * width + width / 2) * 2], 2, 0.001f,
                 "model receives two-frame accumulated motion");

            // Invalidate on resize; old generations are released through actual recording fences.
            width = 23;
            height = 9;
            AllocateFrames();
            Near(Frame(cache, cfg, true, 1, 1.2f)[0], 1.2f, 0.015f, "resize starts fresh history");
            cfg.DlssNrCacheInterval = 1;
            cfg.DlssNrCacheStabilize = 0.5f;
            cfg.DlssNrCacheDespeckle = true;
            cfg.DlssNrCacheTemporal = 0.5f;
            cfg.DlssNrCacheLowTemporal = 0.95f;
            cfg.DlssNrCacheCrossfade = true;
            Frame(cache, cfg, true, 1, 1.05f, true);
            const auto anti = Frame(cache, cfg, true, 1, 1.12f);
            Require(anti[0] < 1.10f && anti[0] > 1.04f, "complete post-SR anti-flicker pipeline");
            Alpha(anti);
        }
        // Many input frames in one executable recording keep every descriptor/CB
        // live. The bounded ring must suspend caching before overwriting any slot.
        cfg.DlssNrCacheInterval = 2;
        cfg.DlssNrCacheModelHistory = 2;
        cfg.DlssNrCacheStabilize = 0;
        cfg.DlssNrCacheTemporal = 0;
        cfg.DlssNrCacheLowTemporal = 0;
        cfg.DlssNrCacheDespeckle = false;
        cfg.DlssNrCacheCrossfade = false;
        {
            DlssNrEditCache_Dx12 cache(device);
            Uniform(original, kSrv, 4, 1);
            auto in = Inputs();
            bool exhausted = false;
            unsigned int recorded = 0;
            for (unsigned int i = 0; i < 256; ++i)
            {
                const bool refresh =
                    cache.BeginFrame(list, cfg, device, width, height, DXGI_FORMAT_R32G32B32A32_FLOAT, false);
                if (!cache.ActiveThisFrame())
                {
                    exhausted = true;
                    break;
                }
                if (refresh)
                {
                    Uniform(target, kUav, 4, 1.2f);
                    Require(cache.CaptureRefresh(list, device, target, original, in), "pending refresh capture");
                }
                else
                {
                    Require(cache.CopyIn(list, original, target), "pending fresh frame");
                    Require(cache.RunCached(list, device, target, keep, in), "pending cached frame");
                }
                cache.EndFrame(list);
                ++recorded;
            }
            const auto pending = cache.GetStatus();
            Require(exhausted && recorded > 8 && recorded < 256, "bounded descriptor exhaustion suspends caching");
            Require(pending.readbacksPending == 4 && pending.statsDropped > 0,
                    "busy readback ring drops telemetry safely");
            std::printf("pending recording: %u frames; readbacks %u; dropped stats %llu; safe suspension\n", recorded,
                        pending.readbacksPending, pending.statsDropped);
            Near(Read(target, kUav, 4)[0], 1.2f, 0.015f, "pending GPU recording kept original descriptors");
            // Completion + real Reset makes those slots reusable, with a refresh.
            Near(Frame(cache, cfg, true, 1, 1.2f)[0], 1.2f, 0.015f, "cache recovers after actual GPU completion");
            // CPU history cannot survive Reset discarding commands that never executed.
            cache.Invalidate();
            auto discarded = DlssNr::GpuLifetime::Begin(list);
            in = Inputs();
            Require(cache.BeginFrame(list, cfg, device, width, height, DXGI_FORMAT_R32G32B32A32_FLOAT, true),
                    "discard seed");
            Uniform(target, kUav, 4, 1.5f);
            Require(cache.CaptureRefresh(list, device, target, original, in), "discard capture");
            cache.EndFrame(list);
            Check(list->Close(), "discard close");
            Check(allocator->Reset(), "discard allocator reset");
            Check(list->Reset(allocator, nullptr), "discard list reset");
            Require(DlssNr::GpuLifetime::Discarded(discarded), "actual discarded recording detected");
            Near(Frame(cache, cfg, true, 1, 1.2f)[0], 1.2f, 0.015f, "discarded GPU history requires fresh NR");
        }
        // Destroy the cache while its commands are still executable/unsubmitted.
        // Its private textures, descriptors, CBs and pipeline remain fence-owned.
        {
            DlssNrEditCache_Dx12 cache(device);
            Uniform(original, kSrv, 4, 1);
            auto in = Inputs();
            cfg.DlssNrCacheInterval = 1;
            cfg.DlssNrCacheLowTemporal = 0.95f;
            Require(cache.BeginFrame(list, cfg, device, width, height, DXGI_FORMAT_R32G32B32A32_FLOAT, true, true),
                    "release test refresh");
            Uniform(target, kUav, 4, 1.2f);
            Require(cache.CaptureRefresh(list, device, target, original, in), "release test capture");
            cache.EndFrame(list);
        }
        Near(Read(target, kUav, 4)[0], 1.2f, 0.015f, "cache destruction retains queued GPU resources");
        Check(list->Close(), "final close");
        list->Release();
        DlssNr::GpuLifetime::Poll();
        Require(DlssNr::GpuLifetime::DrainForShutdown(), "actual queue drain");
        for (auto* resource : owners)
            resource->Release();
        allocator->Release();
        fence->Release();
        queue->Release();
        device->Release();
        std::puts(
            "PASS: production Sky cache cadence, fresh frame, alpha, regional pre-SR, full post-SR anti-flicker, "
            "jitter, accumulated motion, resize, bounded pending slots/readbacks, queued destruction and fence drain");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::printf("FAIL: %s\n", error.what());
        return 1;
    }
}
