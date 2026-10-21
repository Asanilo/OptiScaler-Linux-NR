#include "pch.h"
#include "RegionMask_Dx12.h"

#include <State.h>

#include "precompiled/RegionMask_Shader.h"

bool RegionMask_Dx12::Dispatch(ID3D12GraphicsCommandList* cmdList, ID3D12Resource* edited, ID3D12Resource* original,
                               ID3D12Resource* depth, ID3D12Resource* output, const RegionMaskConstants& constants)
{
    if (!_init || _device == nullptr || cmdList == nullptr || edited == nullptr || original == nullptr ||
        depth == nullptr || output == nullptr)
        return false;

    _counter = (_counter + 1) % REGION_MASK_NUM_OF_HEAPS;
    FrameDescriptorHeap& heap = _frameHeaps[_counter];

    CreateShaderResourceView(_device, edited, heap.GetSrvCPU(0));
    CreateShaderResourceView(_device, original, heap.GetSrvCPU(1));

    // Depth in a readable view: the generic typeless translation turns
    // R32_FLOAT_X8X24 into a depth-stencil format, invalid for a shader view.
    DXGI_FORMAT view = DXGI_FORMAT_UNKNOWN;

    switch (depth->GetDesc().Format)
    {
    case DXGI_FORMAT_R32G8X24_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
    case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
        view = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
        break;
    case DXGI_FORMAT_R24G8_TYPELESS:
    case DXGI_FORMAT_D24_UNORM_S8_UINT:
    case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
        view = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        break;
    case DXGI_FORMAT_R32_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT:
        view = DXGI_FORMAT_R32_FLOAT;
        break;
    case DXGI_FORMAT_R16_TYPELESS:
    case DXGI_FORMAT_D16_UNORM:
        view = DXGI_FORMAT_R16_UNORM;
        break;
    default:
        break;
    }

    if (view != DXGI_FORMAT_UNKNOWN)
        CreateShaderResourceView(_device, depth, heap.GetSrvCPU(2), view);
    else
        CreateShaderResourceView(_device, depth, heap.GetSrvCPU(2));

    CreateUnorderedAccessView(_device, output, heap.GetUavCPU(0), 0);

    if (!CreateConstantsBuffer(_device, _constantBuffers[_counter], constants, heap.GetCbvCPU(0)))
    {
        LOG_ERROR("[{0}] Failed to create a constants buffer", _name);
        return false;
    }

    ID3D12DescriptorHeap* heaps[] = { heap.GetHeapCSU() };
    cmdList->SetDescriptorHeaps(_countof(heaps), heaps);
    cmdList->SetComputeRootSignature(_rootSignature);
    cmdList->SetPipelineState(_pipelineState);
    cmdList->SetComputeRootDescriptorTable(0, heap.GetTableGPUStart());
    cmdList->Dispatch((constants.Width + 7) / 8, (constants.Height + 7) / 8, 1);
    return true;
}

RegionMask_Dx12::RegionMask_Dx12(std::string name, ID3D12Device* device) : Shader_Dx12(name, device)
{
    if (device == nullptr)
    {
        LOG_ERROR("device is nullptr!");
        return;
    }

    if (!SetupRootSignature(device, 3, 1, 1))
    {
        LOG_ERROR("[{0}] Failed to setup root signature", _name);
        return;
    }

    D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(RegionMaskConstants));
    auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

    for (auto& buffer : _constantBuffers)
    {
        if (device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ,
                                            nullptr, IID_PPV_ARGS(&buffer)) != S_OK)
        {
            LOG_ERROR("[{0}] Failed to create a constant buffer", _name);
            return;
        }
    }

    if (!CreateComputePipeline(device, &_pipelineState, RegionMask_cso, sizeof(RegionMask_cso), nullptr))
    {
        LOG_ERROR("[{0}] Failed to create compute pipeline", _name);
        return;
    }

    _init = InitHeaps(device, _frameHeaps, REGION_MASK_NUM_OF_HEAPS);
}

RegionMask_Dx12::~RegionMask_Dx12()
{
    if (State::Instance().isShuttingDown)
        return;

    for (auto& buffer : _constantBuffers)
    {
        if (buffer != nullptr)
        {
            buffer->Release();
            buffer = nullptr;
        }
    }

    if (!_init)
        return;

    for (auto& heap : _frameHeaps)
        heap.ReleaseHeaps();
}
