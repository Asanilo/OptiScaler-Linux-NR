#pragma once
#include "SysUtils.h"
#include <d3d12.h>
#include <dlssnr/NrGpuLifetime.h>
#include <vector>

class GpuTime_Dx12
{
    static constexpr int QUERY_BUFFER_COUNT = 16;
    int _bufferCount = 3;

    ID3D12QueryHeap* _queryHeap = nullptr;
    ID3D12Resource* _readbackBuffer = nullptr;
    std::array<bool, QUERY_BUFFER_COUNT> _trigger {};

    int _currentFrameIndex = 0;
    bool _init = false;
    bool _nrFenced = false;
    bool _recordingStarted = false;
    std::array<DlssNr::GpuLifetime::Token, QUERY_BUFFER_COUNT> _completion {};
    std::array<uint64_t, QUERY_BUFFER_COUNT> _tags {};
    uint64_t _sampleSequence = 0;

  public:
    GpuTime_Dx12(ID3D12Device* device, bool nrFenced = false);
    ~GpuTime_Dx12();

    void Start(ID3D12GraphicsCommandList* cmdList);
    void End(ID3D12GraphicsCommandList* cmdList, uint64_t tag = 0);

    struct Sample
    {
        uint64_t tag;
        double ms;
    };
    // NR only: drain every ready query once, without tying sampling to ring phase.
    std::vector<Sample> ReadCompletedGpuTimes();

    std::optional<double> ReadGpuTime(ID3D12CommandQueue* commandQueue);
};

class ScopedGpuTime_Dx12
{
    GpuTime_Dx12* _gpuTime;
    ID3D12GraphicsCommandList* _cmdList;

  public:
    ScopedGpuTime_Dx12(GpuTime_Dx12* gpuTime, ID3D12GraphicsCommandList* cmdList) : _gpuTime(gpuTime), _cmdList(cmdList)
    {
        if (_gpuTime && _cmdList)
            _gpuTime->Start(_cmdList);
    }

    ~ScopedGpuTime_Dx12()
    {
        if (_gpuTime && _cmdList)
            _gpuTime->End(_cmdList);
    }
};
