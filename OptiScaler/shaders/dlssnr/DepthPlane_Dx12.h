#pragma once
#include <d3d12.h>

namespace DlssNr
{
inline bool IsPlanarDepth(DXGI_FORMAT f)
{
    return f == DXGI_FORMAT_R32G8X24_TYPELESS || f == DXGI_FORMAT_D32_FLOAT_S8X24_UINT ||
           f == DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS || f == DXGI_FORMAT_R24G8_TYPELESS ||
           f == DXGI_FORMAT_D24_UNORM_S8_UINT || f == DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
}
inline void CreateDepthPlaneSrv(ID3D12Device* device, ID3D12Resource* source, D3D12_CPU_DESCRIPTOR_HANDLE handle)
{
    const auto format = source->GetDesc().Format;
    D3D12_SHADER_RESOURCE_VIEW_DESC view {};
    view.Format = format == DXGI_FORMAT_R24G8_TYPELESS || format == DXGI_FORMAT_D24_UNORM_S8_UINT ||
                          format == DXGI_FORMAT_R24_UNORM_X8_TYPELESS
                      ? DXGI_FORMAT_R24_UNORM_X8_TYPELESS
                      : DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
    view.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    view.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    view.Texture2D.MipLevels = 1;
    view.Texture2D.PlaneSlice = 0;
    device->CreateShaderResourceView(source, &view, handle);
}
} // namespace DlssNr
