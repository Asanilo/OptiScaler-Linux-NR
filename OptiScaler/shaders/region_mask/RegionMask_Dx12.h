#pragma once

#include "RegionMask_Common.h"

#include <d3d12.h>
#include <d3dx/d3dx12.h>
#include <shaders/Shader_Dx12Utils.h>
#include <shaders/Shader_Dx12.h>

#define REGION_MASK_NUM_OF_HEAPS 4

// The region mask post-pass: reads the edited frame, the game's untouched frame
// and depth, and writes the frame with the chosen region given back. Off by
// default; nothing here allocates or runs unless DlssNrRegionMask is 1 or 2.
class RegionMask_Dx12 : public Shader_Dx12
{
  private:
    FrameDescriptorHeap _frameHeaps[REGION_MASK_NUM_OF_HEAPS];
    ID3D12Resource* _constantBuffers[REGION_MASK_NUM_OF_HEAPS] {};
    unsigned int _counter = 0;

  public:
    bool Dispatch(ID3D12GraphicsCommandList* cmdList, ID3D12Resource* edited, ID3D12Resource* original,
                  ID3D12Resource* depth, ID3D12Resource* output, const RegionMaskConstants& constants);

    RegionMask_Dx12(std::string name, ID3D12Device* device);
    ~RegionMask_Dx12();
};
