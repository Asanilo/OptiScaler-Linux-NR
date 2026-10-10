#include "pch.h"
#include "NrStabilizer_Dx12.h"
#include "precompile/NrStabilizer_Shader.h"
#include <shaders/Shader_Dx12Utils.h>

namespace
{
constexpr auto kUav = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
constexpr auto kSrv = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
void Transition(ID3D12GraphicsCommandList* list, ID3D12Resource* resource, D3D12_RESOURCE_STATES from,
                D3D12_RESOURCE_STATES to)
{
    DlssNr::GpuLifetime::Hold(list, resource);
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(resource, from, to);
    list->ResourceBarrier(1, &barrier);
}
} // namespace

NrStabilizer_Dx12::NrStabilizer_Dx12(ID3D12Device* device) : Shader_Dx12("NR Stabilization", device)
{
    if (!SetupRootSignature(device, 6, 3, 1))
        return;
    auto heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto desc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(NrStabilizerConstants));
    for (auto& buffer : constants_)
        if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
                                                   D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&buffer))))
            return;
    if (!CreateComputePipeline(device, &_pipelineState, NrStabilizer_cso, sizeof(NrStabilizer_cso), nullptr))
        return;
    _init = InitHeaps(device, heaps_, kSlots);
}

void NrStabilizer_Dx12::ReleaseResources()
{
    if (fresh_)
        fresh_->Release();
    fresh_ = nullptr;
    for (auto& resource : edit_)
    {
        if (resource)
            resource->Release();
        resource = nullptr;
    }
    for (auto& resource : guide_)
    {
        if (resource)
            resource->Release();
        resource = nullptr;
    }
    Invalidate();
}

NrStabilizer_Dx12::~NrStabilizer_Dx12()
{
    ReleaseResources();
    for (auto* buffer : constants_)
        if (buffer)
            buffer->Release();
}

bool NrStabilizer_Dx12::EnsureResources(ID3D12Resource* target)
{
    const auto desc = target->GetDesc();
    if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D || !desc.Width || !desc.Height ||
        desc.DepthOrArraySize != 1 || desc.MipLevels != 1 || desc.SampleDesc.Count != 1)
        return false;
    if (fresh_ && width_ == desc.Width && height_ == desc.Height && format_ == desc.Format)
        return true;
    ReleaseResources();
    width_ = static_cast<unsigned int>(desc.Width);
    height_ = desc.Height;
    format_ = desc.Format;
    auto heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    auto make = [&](DXGI_FORMAT format, ID3D12Resource** resource)
    {
        auto texture = CD3DX12_RESOURCE_DESC::Tex2D(format, width_, height_, 1, 1, 1, 0,
                                                    D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        return SUCCEEDED(_device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &texture, kUav, nullptr,
                                                          IID_PPV_ARGS(resource)));
    };
    const auto freshFormat =
        format_ == DXGI_FORMAT_R10G10B10A2_TYPELESS ? DXGI_FORMAT_R10G10B10A2_UNORM : TranslateTypelessFormats(format_);
    if (!make(freshFormat, &fresh_) || !make(DXGI_FORMAT_R16G16B16A16_FLOAT, &edit_[0]) ||
        !make(DXGI_FORMAT_R16G16B16A16_FLOAT, &edit_[1]) || !make(DXGI_FORMAT_R32G32_FLOAT, &guide_[0]) ||
        !make(DXGI_FORMAT_R32G32_FLOAT, &guide_[1]))
    {
        ReleaseResources();
        return false;
    }
    current_ = 0;
    return true;
}

bool NrStabilizer_Dx12::Run(ID3D12GraphicsCommandList* list, ID3D12Resource* target, ID3D12Resource* original,
                            ID3D12Resource* depth, ID3D12Resource* motion, const DlssNrFrameInfo& frame,
                            float whitePoint, float stepLimit, float depthTolerance, float colourTolerance,
                            bool despeckle)
{
    if (!_init || !EnsureResources(target))
    {
        Invalidate();
        return false;
    }
    const auto token = DlssNr::GpuLifetime::Begin(list);
    if (!token)
    {
        Invalidate();
        return false;
    }
    const int claimed = DlssNr::GpuLifetime::ClaimSlot(owners_, kSlots, slot_, token);
    if (claimed < 0)
    {
        Invalidate();
        return false;
    }
    const auto slot = static_cast<unsigned int>(claimed);
    auto& heap = heaps_[slot];
    if (frame.Reset)
        Invalidate();
    NrStabilizerConstants c {};
    c.Width = width_;
    c.Height = height_;
    const auto d = depth->GetDesc(), m = motion->GetDesc();
    c.DepthWidth = static_cast<unsigned int>(d.Width);
    c.DepthHeight = d.Height;
    c.MotionWidth = static_cast<unsigned int>(m.Width);
    c.MotionHeight = m.Height;
    if (frame.RenderSubrectWidth && frame.RenderSubrectHeight)
    {
        c.DepthWidth = std::min(c.DepthWidth, frame.RenderSubrectWidth);
        c.DepthHeight = std::min(c.DepthHeight, frame.RenderSubrectHeight);
        c.MotionWidth = std::min(c.MotionWidth, frame.RenderSubrectWidth);
        c.MotionHeight = std::min(c.MotionHeight, frame.RenderSubrectHeight);
    }
    if (!c.DepthWidth || !c.DepthHeight || !c.MotionWidth || !c.MotionHeight)
    {
        Invalidate();
        return false;
    }
    c.DepthInverted = frame.DepthInverted;
    c.HistoryValid = valid_;
    c.MvScaleX = frame.MvScaleX;
    c.MvScaleY = frame.MvScaleY;
    // Same sign convention as Sky's default JitterSign=-1.
    if (frame.PreUpscale && !frame.MotionJittered && frame.JitterValid && jitterValid_)
    {
        c.JitterDeltaX = (jitterX_ - frame.JitterX) / width_;
        c.JitterDeltaY = (jitterY_ - frame.JitterY) / height_;
    }
    if (frame.PreUpscale && !frame.JitterValid)
        c.HistoryValid = 0;
    jitterX_ = frame.JitterX;
    jitterY_ = frame.JitterY;
    jitterValid_ = frame.PreUpscale && frame.JitterValid;
    c.Epsilon = std::max(whitePoint / 512.0f, 1e-7f);
    c.StepLimit = std::clamp(stepLimit, 0.0f, 4.0f);
    c.DepthTolerance = std::clamp(depthTolerance, 0.005f, 1.0f);
    c.ColourTolerance = std::clamp(colourTolerance, 0.05f, 8.0f);
    c.Despeckle = despeckle;
    if (!CreateConstantsBuffer(_device, constants_[slot], c, heap.GetCbvCPU(0)))
    {
        Invalidate();
        return false;
    }
    const unsigned int next = 1 - current_;
    ID3D12Resource* srvs[] { fresh_, original, depth, motion, edit_[current_], guide_[current_] };
    ID3D12Resource* uavs[] { target, edit_[next], guide_[next] };
    for (unsigned int i = 0; i < 6; ++i)
    {
        CreateShaderResourceView(_device, srvs[i], heap.GetSrvCPU(i));
        DlssNr::GpuLifetime::Hold(token, srvs[i]);
    }
    for (unsigned int i = 0; i < 3; ++i)
    {
        CreateUnorderedAccessView(_device, uavs[i], heap.GetUavCPU(i), 0);
        DlssNr::GpuLifetime::Hold(token, uavs[i]);
    }
    DlssNr::GpuLifetime::Hold(token, constants_[slot]);
    DlssNr::GpuLifetime::Hold(token, heap.GetHeapCSU());
    DlssNr::GpuLifetime::Hold(token, _rootSignature);
    DlssNr::GpuLifetime::Hold(token, _pipelineState);
    Transition(list, target, kUav, D3D12_RESOURCE_STATE_COPY_SOURCE);
    Transition(list, fresh_, kUav, D3D12_RESOURCE_STATE_COPY_DEST);
    list->CopyResource(fresh_, target);
    Transition(list, target, D3D12_RESOURCE_STATE_COPY_SOURCE, kUav);
    Transition(list, fresh_, D3D12_RESOURCE_STATE_COPY_DEST, kSrv);
    Transition(list, edit_[current_], kUav, kSrv);
    Transition(list, guide_[current_], kUav, kSrv);
    ID3D12DescriptorHeap* heaps[] { heap.GetHeapCSU() };
    list->SetDescriptorHeaps(1, heaps);
    list->SetComputeRootSignature(_rootSignature);
    list->SetPipelineState(_pipelineState);
    list->SetComputeRootDescriptorTable(0, heap.GetTableGPUStart());
    list->Dispatch((width_ + 7) / 8, (height_ + 7) / 8, 1);
    Transition(list, fresh_, kSrv, kUav);
    Transition(list, edit_[current_], kSrv, kUav);
    Transition(list, guide_[current_], kSrv, kUav);
    current_ = next;
    valid_ = true;
    return true;
}
