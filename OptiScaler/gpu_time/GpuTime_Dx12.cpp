#include "pch.h"
#include "GpuTime_Dx12.h"

#include <State.h>
#include <algorithm>

#include <include/d3dx/d3dx12.h>

GpuTime_Dx12::GpuTime_Dx12(ID3D12Device* device, bool nrFenced) : _nrFenced(nrFenced)
{
    _bufferCount = nrFenced ? QUERY_BUFFER_COUNT : 3;
    // Create query heap for Start and End timestamps per buffer
    D3D12_QUERY_HEAP_DESC queryHeapDesc = {};
    queryHeapDesc.Count = _bufferCount * 2;
    queryHeapDesc.NodeMask = 0;
    queryHeapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;

    auto result = device->CreateQueryHeap(&queryHeapDesc, IID_PPV_ARGS(&_queryHeap));

    if (result != S_OK)
    {
        LOG_ERROR("CreateQueryHeap error: {:X}", (UINT) result);
        return;
    }

    // Create a readback buffer large enough for all frames
    D3D12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(_bufferCount * 2 * sizeof(UINT64));
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_READBACK;

    result = device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc,
                                             D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&_readbackBuffer));

    if (result != S_OK)
    {
        LOG_ERROR("CreateCommittedResource error: {:X}", (UINT) result);
        return;
    }

    _init = true;
}

GpuTime_Dx12::~GpuTime_Dx12()
{
    SAFE_RELEASE(_queryHeap);
    SAFE_RELEASE(_readbackBuffer);
}

void GpuTime_Dx12::Start(ID3D12GraphicsCommandList* cmdList)
{
    _recordingStarted = false;
    if (_init && _queryHeap != nullptr)
    {
        auto next = (_currentFrameIndex + 1) % _bufferCount;
        if (_nrFenced)
        {
            // Completed but unread slots belong to the consumer, too. Search all
            // slots so one busy recording cannot pin sampling to one cache phase.
            bool found = false;
            for (int offset = 0; offset < _bufferCount; ++offset)
            {
                const auto candidate = (next + offset) % _bufferCount;
                if (DlssNr::GpuLifetime::Discarded(_completion[candidate]))
                    _trigger[candidate] = false;
                if (!_trigger[candidate] && DlssNr::GpuLifetime::Reusable(_completion[candidate]))
                {
                    next = candidate;
                    found = true;
                    break;
                }
            }
            if (!found)
                return; // Bounded telemetry: do not stall the game or overwrite queries.
            _completion[next] = DlssNr::GpuLifetime::Begin(cmdList);
            if (!_completion[next])
                return;
            DlssNr::GpuLifetime::Hold(_completion[next], _queryHeap);
            DlssNr::GpuLifetime::Hold(_completion[next], _readbackBuffer);
        }
        _currentFrameIndex = next;
        _trigger[next] = false;
        _recordingStarted = true;

        cmdList->EndQuery(_queryHeap, D3D12_QUERY_TYPE_TIMESTAMP, _currentFrameIndex * 2);
    }
}

void GpuTime_Dx12::End(ID3D12GraphicsCommandList* cmdList, uint64_t tag)
{
    if (_init && _queryHeap != nullptr && _recordingStarted)
    {
        cmdList->EndQuery(_queryHeap, D3D12_QUERY_TYPE_TIMESTAMP, _currentFrameIndex * 2 + 1);

        cmdList->ResolveQueryData(_queryHeap, D3D12_QUERY_TYPE_TIMESTAMP, _currentFrameIndex * 2, 2, _readbackBuffer,
                                  _currentFrameIndex * 2 * sizeof(UINT64));

        _tags[_currentFrameIndex] = tag ? tag : ++_sampleSequence;
        _trigger[_currentFrameIndex] = true;
        _recordingStarted = false;
    }
}

std::optional<double> GpuTime_Dx12::ReadGpuTime(ID3D12CommandQueue* commandQueue)
{
    std::optional<double> elapsedTimeMs = std::nullopt;

    if (!_init || _queryHeap == nullptr || _readbackBuffer == nullptr)
        return elapsedTimeMs;

    if (_nrFenced)
    {
        const auto samples = ReadCompletedGpuTimes();
        if (!samples.empty())
            return samples.back().ms;
        return elapsedTimeMs;
    }

    // Preserve the existing three-slot path for non-NR upscaler timers.
    uint32_t previousFrameIndex = (_currentFrameIndex + 1) % _bufferCount;

    if (!_trigger[previousFrameIndex] ||
        (_nrFenced && !DlssNr::GpuLifetime::ReadbackReady(_completion[previousFrameIndex])))
        return elapsedTimeMs;

    UINT64* timestampData {};

    // Tell it which timestamps we will be reading
    D3D12_RANGE readRange = { previousFrameIndex * 2 * sizeof(UINT64), (previousFrameIndex * 2 + 2) * sizeof(UINT64) };
    if (FAILED(_readbackBuffer->Map(0, &readRange, reinterpret_cast<void**>(&timestampData))))
        return elapsedTimeMs;

    // CPU doesn't write anything
    D3D12_RANGE writeRange = { 0, 0 };

    if (timestampData != nullptr)
    {
        // Get the GPU timestamp frequency (ticks per second)
        UINT64 gpuFrequency = 0;
        if (_nrFenced)
            gpuFrequency = DlssNr::GpuLifetime::TimestampFrequency(_completion[previousFrameIndex]);
        else
            commandQueue->GetTimestampFrequency(&gpuFrequency);
        if (!gpuFrequency)
        {
            _readbackBuffer->Unmap(0, &writeRange);
            return elapsedTimeMs;
        }

        // Calculate elapsed time in milliseconds
        UINT64 startTime = timestampData[previousFrameIndex * 2];
        UINT64 endTime = timestampData[previousFrameIndex * 2 + 1];

        if (endTime < startTime)
        {
            _readbackBuffer->Unmap(0, &writeRange);
            return elapsedTimeMs;
        }

        elapsedTimeMs = (endTime - startTime) / static_cast<double>(gpuFrequency) * 1000.0;
    }
    else
    {
        LOG_WARN("timestampData is null!");
    }

    _readbackBuffer->Unmap(0, &writeRange);
    if (_nrFenced)
        _trigger[previousFrameIndex] = false;

    return elapsedTimeMs;
}

std::vector<GpuTime_Dx12::Sample> GpuTime_Dx12::ReadCompletedGpuTimes()
{
    std::vector<Sample> samples;
    if (!_nrFenced || !_init || !_readbackBuffer)
        return samples;
    for (int slot = 0; slot < _bufferCount; ++slot)
    {
        if (!_trigger[slot])
            continue;
        if (DlssNr::GpuLifetime::Discarded(_completion[slot]))
        {
            _trigger[slot] = false;
            continue;
        }
        if (!DlssNr::GpuLifetime::ReadbackReady(_completion[slot]))
            continue;
        const auto frequency = DlssNr::GpuLifetime::TimestampFrequency(_completion[slot]);
        UINT64* data = nullptr;
        D3D12_RANGE range { slot * 2 * sizeof(UINT64), (slot * 2 + 2) * sizeof(UINT64) };
        if (FAILED(_readbackBuffer->Map(0, &range, reinterpret_cast<void**>(&data))))
            continue;
        if (data && frequency && data[slot * 2 + 1] >= data[slot * 2])
            samples.push_back({ _tags[slot], (data[slot * 2 + 1] - data[slot * 2]) * 1000.0 / frequency });
        D3D12_RANGE noWrite { 0, 0 };
        _readbackBuffer->Unmap(0, &noWrite);
        _trigger[slot] = false;
    }
    std::sort(samples.begin(), samples.end(), [](const Sample& a, const Sample& b) { return a.tag < b.tag; });
    return samples;
}
