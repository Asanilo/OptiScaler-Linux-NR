// Included after the GPU fixture helpers. Uses the production constants, DXBC,
// SRV/UAV helpers and root layout. Vendor inference is not part of this test.
class MotionShaderFixture : public Shader_Dx12
{
    FrameDescriptorHeap heap_;
    ID3D12Resource* constants_ = nullptr;

  public:
    MotionShaderFixture() : Shader_Dx12("motion-test", device)
    {
        D3D12_STATIC_SAMPLER_DESC sampler {};
        sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampler.MaxLOD = D3D12_FLOAT32_MAX;
        Require(SetupRootSignature(device, 5, 2, 1, 0, 0, 1, &sampler), "motion root");
        Require(CreateComputePipeline(device, &_pipelineState, DlssNr_cso, sizeof(DlssNr_cso), nullptr),
                "motion production DXBC pipeline");
        Require(InitHeaps(device, &heap_, 1), "motion descriptors");
        constants_ = Buffer(sizeof(DlssNrConstants), D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
    }
    void Run(ID3D12Resource* src, ID3D12Resource* dst, float x, float y, uint32_t mode = DlssNrMode_Motion)
    {
        DlssNrConstants c {};
        c.Mode = mode;
        c.Width = static_cast<unsigned int>(src->GetDesc().Width);
        c.Height = src->GetDesc().Height;
        c.MotionOffsetX = x;
        c.MotionOffsetY = y;
        void* mapped = nullptr;
        Check(constants_->Map(0, nullptr, &mapped), "motion constants map");
        std::memcpy(mapped, &c, sizeof(c));
        constants_->Unmap(0, nullptr);
        D3D12_CONSTANT_BUFFER_VIEW_DESC cb { constants_->GetGPUVirtualAddress(), sizeof(c) };
        device->CreateConstantBufferView(&cb, heap_.GetCbvCPU(0));
        for (unsigned int i = 0; i < 5; ++i)
            if (mode == DlssNrMode_Depth)
                DlssNr::CreateDepthPlaneSrv(device, src, heap_.GetSrvCPU(i));
            else
                CreateShaderResourceView(device, src, heap_.GetSrvCPU(i));
        for (unsigned int i = 0; i < 2; ++i)
            CreateUnorderedAccessView(device, dst, heap_.GetUavCPU(i), 0);
        ID3D12DescriptorHeap* heaps[] { heap_.GetHeapCSU() };
        list->SetDescriptorHeaps(1, heaps);
        list->SetComputeRootSignature(_rootSignature);
        list->SetPipelineState(_pipelineState);
        list->SetComputeRootDescriptorTable(0, heap_.GetTableGPUStart());
        list->Dispatch((c.Width + 7) / 8, (c.Height + 7) / 8, 1);
    }
};
void MotionShaderTest(bool broken)
{
    auto* input = Texture(DXGI_FORMAT_R32G32_FLOAT, kSrv);
    auto* output = Texture(DXGI_FORMAT_R32G32_FLOAT, kUav);
    MotionShaderFixture shader;
    const float jx = 0.25f, jy = 0.555555582f, sx = -640, sy = 360;
    for (int run = 0; run < 2; ++run)
    {
        // Both a static half and a moving half, with non-square, non-threadgroup
        // dimensions. No resampling or suppression of real motion is allowed.
        Fill(input, kSrv, 2, [&](unsigned int x, unsigned int, unsigned int c)
             { return c == 0 ? ((x < width / 2 ? 0.0f : 3.0f) + jx) / sx : (-7.0f + jy) / sy; });
        shader.Run(input, output, broken ? jx / sx : -jx / sx, -jy / sy);
        const auto values = Read(output, kUav, 2);
        for (unsigned int y = 0; y < height; ++y)
            for (unsigned int x = 0; x < width; ++x)
            {
                const auto i = (y * width + x) * 2;
                Require(std::abs(values[i] * sx - (x < width / 2 ? 0.0f : 3.0f)) < 1e-5f &&
                            std::abs(values[i + 1] * sy + 7.0f) < 1e-5f,
                        "post-SR correction preserves real motion");
            }
        const auto untouched = Read(input, kSrv, 2);
        Require(std::abs(untouched[0] * sx - jx) < 1e-6f, "game motion remains unchanged");
    }
    if (!broken)
        std::puts("PASS: production post-SR motion shader, signed axes, static/moving pixels and immutable input");
}
