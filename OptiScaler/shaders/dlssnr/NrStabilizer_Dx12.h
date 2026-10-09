#pragma once
#include <shaders/Shader_Dx12.h>
#include <dlssnr/NrGpuLifetime.h>
#include "DlssNr_Common.h"
#include "NrStabilizer_Common.h"

class NrStabilizer_Dx12 : public Shader_Dx12
{
    static constexpr unsigned int kSlots = 48;
    FrameDescriptorHeap heaps_[kSlots];
    ID3D12Resource* constants_[kSlots] {};
    std::weak_ptr<DlssNr::Lifetime::Recording> owners_[kSlots];
    unsigned int slot_ = 0;
    ID3D12Resource* fresh_ = nullptr;
    ID3D12Resource* edit_[2] {};
    ID3D12Resource* guide_[2] {};
    unsigned int width_ = 0, height_ = 0, current_ = 0;
    DXGI_FORMAT format_ = DXGI_FORMAT_UNKNOWN;
    bool valid_ = false, jitterValid_ = false;
    float jitterX_ = 0, jitterY_ = 0;
    bool EnsureResources(ID3D12Resource* target);
    void ReleaseResources();

  public:
    explicit NrStabilizer_Dx12(ID3D12Device* device);
    ~NrStabilizer_Dx12();
    void Invalidate() { valid_ = false; jitterValid_ = false; }
    bool Run(ID3D12GraphicsCommandList* list, ID3D12Resource* target, ID3D12Resource* original,
             ID3D12Resource* depth, ID3D12Resource* motion, const DlssNrFrameInfo& frame,
             float whitePoint, float stepLimit, float depthTolerance, float colourTolerance, bool despeckle);
};
