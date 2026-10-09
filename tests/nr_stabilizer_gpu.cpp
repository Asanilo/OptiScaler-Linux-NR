// Execute the exact production DXBC and constants on a NVIDIA D3D12 device.
// Synthetic shader invariants, not a game appearance/performance acceptance.
#define NOMINMAX
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <d3dcompiler.h>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <vector>
#include "../OptiScaler/shaders/dlssnr/NrStabilizer_Common.h"
#include "../OptiScaler/shaders/dlssnr/precompile/NrStabilizer_Shader.h"

void Check(HRESULT result, const char* operation)
{
    if (FAILED(result))
    {
        std::printf("%s: %08lx\n", operation, static_cast<unsigned long>(result));
        throw std::runtime_error(operation);
    }
}
void Require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}
constexpr unsigned int kSize = 16;
ID3D12Device* device = nullptr;
ID3D12CommandQueue* queue = nullptr;
ID3D12CommandAllocator* allocator = nullptr;
ID3D12GraphicsCommandList* list = nullptr;
ID3D12Fence* fence = nullptr;
uint64_t fenceValue = 0;
ID3D12DescriptorHeap* descriptors = nullptr;
ID3D12RootSignature* root = nullptr;
ID3D12PipelineState* pipeline = nullptr;
ID3D12Resource* fresh = nullptr;
ID3D12Resource* original = nullptr;
ID3D12Resource* depth = nullptr;
ID3D12Resource* motion = nullptr;
ID3D12Resource* edit[2] {};
ID3D12Resource* guide[2] {};
ID3D12Resource* target = nullptr;
ID3D12Resource* constants = nullptr;
std::vector<ID3D12Resource*> resources;

ID3D12Resource* Buffer(UINT64 bytes, D3D12_HEAP_TYPE type, D3D12_RESOURCE_STATES state)
{
    D3D12_HEAP_PROPERTIES heap {};
    heap.Type = type;
    D3D12_RESOURCE_DESC desc {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = bytes;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    ID3D12Resource* resource = nullptr;
    Check(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, state, nullptr, IID_PPV_ARGS(&resource)),
          "buffer");
    resources.push_back(resource);
    return resource;
}
ID3D12Resource* Texture(DXGI_FORMAT format, D3D12_RESOURCE_STATES state)
{
    D3D12_HEAP_PROPERTIES heap {};
    heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC desc {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = kSize;
    desc.Height = kSize;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    ID3D12Resource* resource = nullptr;
    Check(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, state, nullptr, IID_PPV_ARGS(&resource)),
          "texture");
    resources.push_back(resource);
    return resource;
}
void Transition(ID3D12Resource* resource, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to)
{
    D3D12_RESOURCE_BARRIER b {};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition.pResource = resource;
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    b.Transition.StateBefore = from;
    b.Transition.StateAfter = to;
    list->ResourceBarrier(1, &b);
}
void Submit()
{
    Check(list->Close(), "close");
    ID3D12CommandList* commands[] { list };
    queue->ExecuteCommandLists(1, commands);
    Check(queue->Signal(fence, ++fenceValue), "signal");
    HANDLE event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    Require(event != nullptr, "event");
    Check(fence->SetEventOnCompletion(fenceValue, event), "fence event");
    const auto wait = WaitForSingleObject(event, 30000);
    CloseHandle(event);
    Require(wait == WAIT_OBJECT_0, "GPU timeout");
    Check(allocator->Reset(), "allocator reset");
    Check(list->Reset(allocator, nullptr), "list reset");
}
void Fill(ID3D12Resource* texture, unsigned int channels, float value, bool checker = false)
{
    const auto desc = texture->GetDesc();
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout {};
    UINT64 bytes = 0;
    device->GetCopyableFootprints(&desc, 0, 1, 0, &layout, nullptr, nullptr, &bytes);
    auto* upload = Buffer(bytes, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
    char* data = nullptr;
    D3D12_RANGE noRead { 0, 0 };
    Check(upload->Map(0, &noRead, reinterpret_cast<void**>(&data)), "upload map");
    std::memset(data, 0, static_cast<size_t>(bytes));
    for (unsigned int y = 0; y < kSize; ++y)
        for (unsigned int x = 0; x < kSize; ++x)
        {
            auto* p = reinterpret_cast<float*>(data + y * layout.Footprint.RowPitch) + x * channels;
            for (unsigned int c = 0; c < channels; ++c)
                p[c] = c == 3 ? 0.75f : checker ? ((x & 1u) ? 2.0f : 1.0f) : value;
        }
    upload->Unmap(0, nullptr);
    Transition(texture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
    D3D12_TEXTURE_COPY_LOCATION dst {};
    dst.pResource = texture;
    dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    D3D12_TEXTURE_COPY_LOCATION src {};
    src.pResource = upload;
    src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    src.PlacedFootprint = layout;
    list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
    Transition(texture, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
}

void Frame(unsigned int previous, NrStabilizerConstants c, float raw, float orig, float z, float mv, float expected,
           const char* label, bool checker = false)
{
    Fill(fresh, 4, raw, checker);
    Fill(original, 4, orig);
    Fill(depth, 1, z);
    Fill(motion, 2, mv);
    void* data = nullptr;
    D3D12_RANGE noRead { 0, 0 };
    Check(constants->Map(0, &noRead, &data), "constant map");
    std::memcpy(data, &c, sizeof(c));
    constants->Unmap(0, nullptr);
    const auto increment = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    auto cpu = descriptors->GetCPUDescriptorHandleForHeapStart();
    ID3D12Resource* srvs[] { fresh, original, depth, motion, edit[previous], guide[previous] };
    for (auto* resource : srvs)
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC srv {};
        srv.Format = resource->GetDesc().Format;
        srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srv.Texture2D.MipLevels = 1;
        srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        device->CreateShaderResourceView(resource, &srv, cpu);
        cpu.ptr += increment;
    }
    ID3D12Resource* uavs[] { target, edit[1 - previous], guide[1 - previous] };
    for (auto* resource : uavs)
    {
        D3D12_UNORDERED_ACCESS_VIEW_DESC uav {};
        uav.Format = resource->GetDesc().Format;
        uav.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        device->CreateUnorderedAccessView(resource, nullptr, &uav, cpu);
        cpu.ptr += increment;
    }
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbv { constants->GetGPUVirtualAddress(), sizeof(c) };
    device->CreateConstantBufferView(&cbv, cpu);
    Transition(edit[previous], D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    Transition(guide[previous], D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    ID3D12DescriptorHeap* heaps[] { descriptors };
    list->SetDescriptorHeaps(1, heaps);
    list->SetComputeRootSignature(root);
    list->SetPipelineState(pipeline);
    list->SetComputeRootDescriptorTable(0, descriptors->GetGPUDescriptorHandleForHeapStart());
    list->Dispatch(2, 2, 1);
    Transition(edit[previous], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    Transition(guide[previous], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    const auto desc = target->GetDesc();
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout {};
    UINT64 bytes = 0;
    device->GetCopyableFootprints(&desc, 0, 1, 0, &layout, nullptr, nullptr, &bytes);
    auto* readback = Buffer(bytes, D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COPY_DEST);
    Transition(target, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
    D3D12_TEXTURE_COPY_LOCATION src {};
    src.pResource = target;
    src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    D3D12_TEXTURE_COPY_LOCATION dst {};
    dst.pResource = readback;
    dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    dst.PlacedFootprint = layout;
    list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
    Transition(target, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    Submit();
    float* output = nullptr;
    D3D12_RANGE range { 0, static_cast<SIZE_T>(bytes) };
    Check(readback->Map(0, &range, reinterpret_cast<void**>(&output)), "readback map");
    const float actual = output[0], alpha = output[3];
    readback->Unmap(0, &noRead);
    std::printf("%s: %.6f (expected %.6f), alpha %.3f\n", label, actual, expected, alpha);
    Require(std::isfinite(actual) && std::abs(actual - expected) < 0.003f && alpha == 0.75f, label);
}

int main(int argc, char** argv)
{
    if (argc > 1 && !std::freopen(argv[1], "w", stdout))
        return 2;
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    try
    {
        IDXGIFactory4* factory = nullptr;
        Check(CreateDXGIFactory1(IID_PPV_ARGS(&factory)), "factory");
        for (UINT i = 0; !device; ++i)
        {
            IDXGIAdapter1* adapter = nullptr;
            if (factory->EnumAdapters1(i, &adapter) == DXGI_ERROR_NOT_FOUND)
                break;
            DXGI_ADAPTER_DESC1 desc {};
            adapter->GetDesc1(&desc);
            if (desc.VendorId == 0x10de && !(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE))
            {
                Check(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)), "device");
                std::printf("NVIDIA %04x:%04x\n", desc.VendorId, desc.DeviceId);
            }
            adapter->Release();
        }
        factory->Release();
        Require(device != nullptr, "No NVIDIA hardware device");
        D3D12_COMMAND_QUEUE_DESC q {};
        q.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        Check(device->CreateCommandQueue(&q, IID_PPV_ARGS(&queue)), "queue");
        Check(device->CreateCommandAllocator(q.Type, IID_PPV_ARGS(&allocator)), "allocator");
        Check(device->CreateCommandList(0, q.Type, allocator, nullptr, IID_PPV_ARGS(&list)), "list");
        Check(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)), "fence");
        D3D12_DESCRIPTOR_HEAP_DESC hd {};
        hd.NumDescriptors = 10;
        hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        hd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        Check(device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&descriptors)), "heap");
        D3D12_DESCRIPTOR_RANGE ranges[3] {};
        ranges[0] = { D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 6, 0, 0, 0 };
        ranges[1] = { D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 3, 0, 0, 6 };
        ranges[2] = { D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 0, 9 };
        D3D12_ROOT_PARAMETER parameter {};
        parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        parameter.DescriptorTable = { 3, ranges };
        parameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        D3D12_ROOT_SIGNATURE_DESC rd {};
        rd.NumParameters = 1;
        rd.pParameters = &parameter;
        ID3DBlob* binary = nullptr;
        ID3DBlob* errors = nullptr;
        Check(D3D12SerializeRootSignature(&rd, D3D_ROOT_SIGNATURE_VERSION_1, &binary, &errors), "root serialize");
        Check(device->CreateRootSignature(0, binary->GetBufferPointer(), binary->GetBufferSize(), IID_PPV_ARGS(&root)),
              "root");
        binary->Release();
        if (errors)
            errors->Release();
        D3D12_COMPUTE_PIPELINE_STATE_DESC pd {};
        pd.pRootSignature = root;
        pd.CS = { NrStabilizer_cso, sizeof(NrStabilizer_cso) };
        Check(device->CreateComputePipelineState(&pd, IID_PPV_ARGS(&pipeline)), "production shader pipeline");
        fresh = Texture(DXGI_FORMAT_R32G32B32A32_FLOAT, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        original = Texture(DXGI_FORMAT_R32G32B32A32_FLOAT, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        depth = Texture(DXGI_FORMAT_R32_FLOAT, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        motion = Texture(DXGI_FORMAT_R32G32_FLOAT, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        target = Texture(DXGI_FORMAT_R32G32B32A32_FLOAT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        for (unsigned int i = 0; i < 2; ++i)
        {
            edit[i] = Texture(DXGI_FORMAT_R16G16B16A16_FLOAT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            guide[i] = Texture(DXGI_FORMAT_R32G32_FLOAT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        }
        constants = Buffer(sizeof(NrStabilizerConstants), D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
        NrStabilizerConstants c {};
        c.Width = c.Height = c.DepthWidth = c.DepthHeight = c.MotionWidth = c.MotionHeight = kSize;
        c.MvScaleX = c.MvScaleY = 1;
        c.Epsilon = 1.0f / 512;
        c.StepLimit = 0.5f;
        c.DepthTolerance = 0.1f;
        c.ColourTolerance = 0.5f;
        Frame(0, c, 1, 1, 0.5f, 0, 1, "invalid history preserves current frame");
        c.HistoryValid = 1;
        const float halfStop = (1 + c.Epsilon) * std::sqrt(2.0f) - c.Epsilon;
        Frame(1, c, 4, 1, 0.5f, 0, halfStop, "valid edit step limited to 0.5 stops");
        c.HistoryValid = 0;
        Frame(0, c, 4, 1, 0.5f, 0, 4, "reset uses current frame");
        c.HistoryValid = 1;
        Frame(1, c, 1, 1, 0.01f, 0, 1, "depth discontinuity rejects history");
        Frame(0, c, 4, 10, 0.01f, 0, 4, "colour change rejects history");
        Frame(1, c, 4, 10, 0.01f, 32, 4, "out-of-frame motion rejects history");
        c.HistoryValid = 0;
        Frame(0, c, 1, 1, 0.5f, 0, 1, "seed jitter checker", true);
        c.HistoryValid = 1;
        c.JitterDeltaX = 1.0f / kSize;
        Frame(1, c, 1, 1, 0.5f, 0, halfStop, "jitter delta shifts edit history", true);
        c.HistoryValid = 0;
        c.JitterDeltaX = 0;
        c.DepthInverted = 1;
        Frame(0, c, 2, 1, 0.5f, 0, 2, "reversed depth reset preserves frame");
        for (auto* resource : resources)
            resource->Release();
        Check(list->Close(), "last close");
        pipeline->Release();
        root->Release();
        descriptors->Release();
        list->Release();
        allocator->Release();
        fence->Release();
        queue->Release();
        device->Release();
        std::puts("PASS: production shader current-frame fallback, step bound, depth/colour/motion rejection, jitter "
                  "and alpha");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::printf("FAIL: %s\n", error.what());
        return 1;
    }
}
